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
        "MOBS_X_STUDIO_URL": (Path(__file__).resolve().parent / "fake-studio-live.html").as_uri(),
        "MOBS_TEST_SOURCE": "colour", "MOBS_TEST_CAMERA": "colour",
        "MOBS_NO_PILOT": "1",   # the go-live helper is off unless a scenario turns it on (MOBS_NO_PILOT=0): the older scenarios are about the steps the person follows

        "MOBS_TOKEN_FILE": str(root / "token"), "MOBS_THEME_DIR": str(root / "theme"),
    }
    for key in ("MOBS_FAKE_BACKEND", "MOBS_FAKE_STOP_HANG", "MOBS_FAKE_STREAM_NEVER_LIVE", "MOBS_TEST_BLACK_CARD", "MOBS_TEST_AUDIO", "MOBS_TEST_DESKTOP_AUDIO", "MOBS_SCRIPT", "MOBS_SETTINGS", "MOBS_GRAB", "MOBS_SPLASH", "MOBS_SPLASH_AT", "MOBS_PICK", "MOBS_SCREEN", "MOBS_WINDOW_SIZE"):
        env.pop(key, None)
    case_count = 0

    def _check(name, steps, size="860x560", fake_backend=False, folder_override=None, extra_env=None, ini_extra="", default_preview=False, expect_log=(), forbid_log=(), limit=15):
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
        result = subprocess.run([binary], env=case_env, capture_output=True, text=True, timeout=limit)   # limit: seconds this scenario may take
        output = result.stdout + result.stderr
        if result.returncode or "TypeError:" in output or "ReferenceError:" in output or "Unable to assign" in output:
            raise AssertionError(f"{name} ({size}): exit {result.returncode}\n{output}")
        if ("script: forcequit" if steps.endswith(",forcequit") else "script: quit") not in result.stdout:
            raise AssertionError(f"{name}: script did not complete\n{output}")
        if expect_log or forbid_log:
            log_file = Path(case_env["MOBS_LOG_FILE"])
            log_text = log_file.read_text() if log_file.exists() else ""
            for line in expect_log:
                if line not in log_text:
                    raise AssertionError(f"{name}: the log never said {line!r}\n{log_text[-1500:]}")
            for line in forbid_log:   # something that must NOT have happened
                if line in log_text:
                    raise AssertionError(f"{name}: the log said {line!r}, which must not happen\n{log_text[-1500:]}")
        print(f"PASS {name} ({size})", flush=True)

    failures = []
    setup_env_early = {"MOBS_SETUP": "1"}

    def check(*args, **kwargs):
        # MOBS_UI_KEEP_GOING=1 reports every failing scenario in one run instead of stopping at the first (for the day a screen was redesigned).
        try:
            _check(*args, **kwargs)
        except (AssertionError, subprocess.TimeoutExpired) as error:
            if os.environ.get("MOBS_UI_KEEP_GOING") != "1":
                raise
            failures.append(args[0])
            print(f"FAIL {args[0]}: {str(error)[:700]}", flush=True)

    # EASY MODE: the interface is scaled up to the window (Main.qml, uiScale): as far as the window allows without the canvas getting smaller than 860 by 560. A 5120 pixel wide screen
    # used to get 13 pixel text (Fred, 2026-10-13: "the text in the app is about 4 sizes too small").
    for size, scale in (("860x560", "1.00"), ("1180x720", "1.29"), ("1500x940", "1.68"), ("2560x1000", "1.79"), ("1720x1120", "2.00"), ("5000x2100", "3.00"), ("900x1600", "1.05")):
        check(f"the interface is scaled up to the window: {scale} at {size}, and the canvas is never smaller than 860 by 560", f"wait=0.2,expect=uiScale:{scale},expect=uiCanvasOk:1,checklayout", size, True)
    # Nothing is hard to read: after the scale no text is smaller than 15 pixels in a 1180 by 720 window, or 19 in a 1500 by 940 one, on any screen.
    for size, px in (("1180x720", 15), ("1500x940", 19)):
        check(f"no text under {px} pixels on the ready screen, EXPERT, or the guide ({size})",
              "pick=1,wait=0.2,checktext=%d," % px + ",".join(f"pro=1,tab={n},wait=0.1,checktext={px}" for n in range(7)) + ",pro=0," + ",".join(f"setupstep={n},wait=0.4,checktext={px}" for n in range(8)),
              size, True, extra_env=setup_env_early)
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
        check("choose", "wait=0.1,checklayout", size, extra_env={"MOBS_SCREEN": "1"})
        check("ready mic off", "pick=1,mic=0,wait=0.1,checklayout", size)
        for tab in range(7):
            check(f"expert page {tab}", f"pro=1,tab={tab},wait=0.1,checklayout", size)
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
        check("audio gain page", "openaudio,audiodetails=1,micgain=-60,wait=0.1,checklayout", size)
        check("missing input explains blocked recording", "pick=1,micdevice=missing,start,expect=state:idle,expect=message:unavailable,wait=0.1,checklayout", size)
        check("audio input check error fits", "openaudio,micdevice=missing,miccheck=1,expect=audioProblem:unavailable,wait=0.1,checklayout", size)
        check("NVIDIA quality page", "pro=1,tab=1,hardware=1,wait=0.1,checklayout", size)
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
        check("ready with named mic and live meter", "pick=1,micdevice=test:usb,miccheck=1,desktop=1,wait=0.1,checklayout", size, True)
        check("ready preview and live meter", "pick=1,preview=1,miccheck=1,wait=0.1,expect=previewFrame:1,checklayout", size, True)
        check("recording both audio meters", "pick=1,desktop=1,start,expect=state:recording,wait=0.1,checklayout,pause,expect=state:paused,wait=0.1,checklayout,pro=1,wait=0.1,checklayout", size, True)
    # A take shows what it is recording (fake backend: a flat colour frame). Wide, small, and a tall narrow window like a tiled one.
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "900x1600", "1206x2000"):
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
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
        check("dragging the webcam in the preview moves it and the layout still fits (webcam window off)", "pick=2,preview=1,wait=0.4,expect=camMoved:0,dragcam=-40:30,expect=camMoved:1,wait=0.2,checklayout", size, True, ini_extra="pip=false\n")
    check("the webcam can be dragged while recording (webcam window off)", "pick=2,start,expect=state:recording,wait=0.5,dragcam=-60:40,expect=camMoved:1,expect=state:recording,wait=0.2,checklayout", fake_backend=True, ini_extra="pip=false\n")
    # The webcam as a window ON the screen (the default for screen + me): SUPER + drag moves it, so there is no handle inside the picture.
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
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
    def sources_env(query=""):   # the guide's X page reads the Sources page of a stand-in Live Studio, with the helper switched on and quick
        return {"MOBS_NO_PILOT": "0", "MOBS_PILOT_TICK_MS": "250", "MOBS_X_SOURCES_URL": (Path(__file__).resolve().parent / "fake-livestudio-sources.html").as_uri() + ("?" + query if query else "")}
    check("the setup guide walks from welcome to FINISH and keeps what was chosen",
          "wait=0.5,expect=setupOpen:1,expect=setupStep:0,click=setupNext,expect=setupStep:1,expect=cameraLocked:0,click=setupLockCamera,expect=cameraLocked:1,click=setupNext,expect=setupStep:2,"
          "click=setupLockMic,expect=micLocked:1,click=setupNext,expect=setupStep:3,click=setupShowScreen,wait=0.8,expect=previewFrame:1,click=setupNext,expect=setupStep:4,wait=0.5,expect=pipShown:1,click=setupNext,"
          "expect=setupStep:5,expect=streamReady:0,wait=6,expect=sourcePhase:done,expect=xSignedIn:1,expect=streamReady:1,expect=sourceName:Gus,click=setupNext,expect=setupStep:6,click=setupNext,expect=setupStep:7,click=setupNext,"
          "expect=setupOpen:0,expect=setupDone:1,expect=setupStep:0",
          fake_backend=True, extra_env=setup_env | sources_env(), limit=40,
          expect_log=("camera locked in: Logitech BRIO", "microphone locked in", "[source] done:", "signed in to X inside MOBS", "guided setup finished"))
    check("the setup guide opens again where the person stopped", "wait=0.5,expect=setupOpen:1,expect=setupStep:3,click=setupBack,expect=setupStep:2", fake_backend=True, extra_env=setup_env, ini_extra="setupStep=3\n")
    check("going back from the webcam page keeps the shared picture for the screen page, and lets go of the camera only at the camera page, which opens it again",
          "wait=0.5,expect=setupOpen:1,expect=setupStep:4,wait=0.6,expect=pipShown:1,click=setupBack,expect=setupStep:3,wait=0.6,expect=pipShown:1,"
          "click=setupBack,expect=setupStep:2,wait=0.4,expect=pipShown:1,click=setupBack,expect=setupStep:1,wait=0.6,expect=pipShown:0", fake_backend=True, extra_env=setup_env, ini_extra="setupStep=4\n")
    check("a mark on the ready screen opens the guide at the page that fixes it", "wait=0.4,expect=setupOpen:0,click=checkMic,expect=setupOpen:1,expect=setupStep:2", fake_backend=True)
    # The microphone list showed three rows with no sign of more: the Maono, the fifth row, was "not an option" (Fred, 2026-10-13). Every microphone is in view when the page has room,
    # and a list that is still longer scrolls in place with a bar beside it.
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
        check("the microphone page lists every microphone: five rows, the system default and four microphones, are all in view",
              "setupstep=2,wait=0.4,expect=setupMicRows:5,expect=setupMicAllShown:1,checklayout", size, True, extra_env=setup_env | {"MOBS_FAKE_MORE_MICS": "1"})
        check("a microphone list longer than the page scrolls in place and the page still fits",
              "setupstep=2,wait=0.4,expect=setupMicRows:25,expect=setupMicAllShown:0,checklayout", size, True, extra_env=setup_env | {"MOBS_FAKE_MORE_MICS": "21"})
    check("the first row of the microphone list says which microphone the system default is",
          "setupstep=2,wait=0.4,expect=setupMicFirst:USB microphone", fake_backend=True, extra_env=setup_env | {"MOBS_FAKE_DEFAULT_SOURCE": "test:usb"})
    check("the camera page shows every camera it found", "setupstep=1,wait=0.4,expect=setupCameraAllShown:1,checklayout", fake_backend=True, extra_env=setup_env)
    # Your X account, one page: MOBS signs in with the person (in the box), finds their source on X (or makes one) and takes the stream key AND the server address from X. The stand-in page
    # (fake-livestudio-sources.html) has the structure of the real Sources page.
    check("the X page finds the source on X and takes its key and address from X, with nothing copied by hand",
          "setupstep=5,wait=6,expect=sourcePhase:done,expect=sourceName:Gus,expect=streamKeyIs:FAKEKEY00GUS,expect=streamUrlHas:va.fake.example,expect=xSignedIn:1,expect=streamReady:1,checklayout",
          fake_backend=True, extra_env=setup_env | sources_env(), limit=30,
          expect_log=("[source] reading: Reading your stream key from X", "[source] done: Your source, Gus, and its key are saved"), forbid_log=("FAKEKEY",))   # the key is never in the log
    check("with no source on X the page makes one called MOBS and uses the address X shows for it, not a built-in one",
          "setupstep=5,wait=9,expect=sourcePhase:done,expect=sourceName:MOBS,expect=streamKeyIs:FAKEKEY0MOBS,expect=streamUrlHas:or.fake.example,expect=streamReady:1",
          fake_backend=True, extra_env=setup_env | sources_env("empty=1"), limit=35, expect_log=("[source] creating: You have no source on X yet",))
    check("with two sources the one the person named is the one whose key is taken",
          "sourcename=Studio,setupstep=5,wait=6,expect=sourcePhase:done,expect=streamKeyIs:FAKEKEY0STUD,expect=sourceName:Studio", fake_backend=True, extra_env=setup_env | sources_env("two=1"), limit=30)
    check("a signed-out Live Studio waits for the person to sign in in the box, then carries on by itself",
          "setupstep=5,wait=1.5,expect=sourcePhase:signin,expect=xSignedIn:0,expect=streamReady:0,wait=7,expect=sourcePhase:done,expect=xSignedIn:1,expect=streamReady:1", fake_backend=True, extra_env=setup_env | sources_env("login=3000"), limit=35)
    check("when the key stays behind its dots the page says so and offers to paste it, and a web address is refused as a key",
          "setupstep=5,wait=6,expect=sourcePhase:failed,expect=streamReady:0,wait=0.2,clip=rtmp://va.pscp.tv:80/x,click=setupPasteKey,wait=0.2,expect=streamReady:0,clip=abc123TESTKEY,click=setupPasteKey,expect=streamReady:1,click=setupNext,expect=setupStep:6",
          fake_backend=True, extra_env=setup_env | sources_env("nokey=1"), limit=35)
    check("when Live Studio never opens the page says so and the person can leave it", "setupstep=5,wait=5,expect=sourcePhase:failed,expect=streamReady:0,checklayout,click=setupSkip,expect=setupStep:6",
          fake_backend=True, extra_env=setup_env | {"MOBS_X_SOURCES_URL": (Path(__file__).resolve().parent / "fake-studio-quiet.html").as_uri(), "MOBS_PILOT_WAIT_MS": "2500", "MOBS_PILOT_TICK_MS": "250"}, limit=30)
    check("X's sign-in page is told passkeys do not exist, so it never waits for one",
          "setupstep=5,wait=2.5,expect=studioTitle:passkey=NotAllowedError/gone", fake_backend=True, extra_env=setup_env | {"MOBS_X_SOURCES_URL": (Path(__file__).resolve().parent / "fake-login.html").as_uri(), "MOBS_PILOT_TICK_MS": "250"})
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
        check("the X page fits while it works and when it gave up", "setupstep=5,wait=1.2,checklayout,wait=5,checklayout", size, True, extra_env=setup_env | sources_env("nokey=1"), limit=30)
    check("signing in to X is noticed from its home page only", "expect=xSignedIn:0,xurl=https://x.com/i/flow/login,expect=xSignedIn:0,xurl=https://evil.example/home,expect=xSignedIn:0,xurl=https://x.com/home,expect=xSignedIn:1",
          fake_backend=True, extra_env=setup_env, expect_log=("signed in to X inside MOBS",))
    check("the done page fits with the name of the source", "setupstep=7,wait=0.3,sourcename=Gus,expect=sourceName:Gus,checklayout", fake_backend=True, extra_env=setup_env)
    check("the go-live window names the source to pick",
          "streamurl=rtmp://live.example/x,streamkey=test-key-123,sourcename=Gus,pick=1,arm=1,start,expect=state:recording,wait=0.6,expect=mainHeld:1,expect=goLiveSteps:Gus. A green dot,checklayout", fake_backend=True)
    check("the go-live window still reads well when no source name was given",
          "streamurl=rtmp://live.example/x,streamkey=test-key-123,pick=1,arm=1,start,expect=state:recording,wait=0.6,expect=mainHeld:1,expect=goLiveSteps:Pick your source. A green dot,checklayout", fake_backend=True)
    # The chat of the person's own livestream, as X shows it: a Live Studio link (Copy URL) attaches it under the webcam, and the card shows only the Live Chat part.
    check("a Live Studio link attaches the chat under the webcam, and the card shows only its Live Chat",
          "pick=2,preview=1,wait=0.4,expect=chatWanted:0,xattach=https://x.com/i/live-studio/1PJqrNXnQywxb,expect=chatXId:1PJqrNXnQywxb,expect=chatWanted:1,wait=6.5,expect=xCrop:shown,wait=0.8,expect=xFresh:1,expect=xRegionTall:180-300,expect=xPixels:ff00ff=none,expect=xPixels:123456=none,expect=xPixels:0a6e5c=some,wait=1.2,checkpip",
          fake_backend=True, expect_log=("X chat attached", "the chat card as drawn", "it is drawing"), forbid_log=("BLACK, the card is drawing nothing",))   # the card looks at its own pixels and says so
    check("a chat card that looks black is not left black: the page is asked for again, and the card is looked at again until it draws",
          "pick=2,preview=1,wait=0.4,xattach=https://x.com/i/live-studio/1PJqrNXnQywxb,expect=chatXId:1PJqrNXnQywxb,wait=6.5,expect=xCrop:shown,wait=12,expect=xPixels:0a6e5c=some", fake_backend=True,
          extra_env={"MOBS_TEST_BLACK_CARD": "1"}, limit=45,
          expect_log=("the card is black: asking X's page again (1 of 3)", "BLACK, the card is drawing nothing", "it is drawing"))   # the first look is made to report black
    live_setup = "streamurl=rtmp://live.example/x,streamkey=test-key-123,"   # a saved address and key, for every scenario that goes live
    check("a livestream nobody has written in yet shows the heading, at the least height",
          "pick=2,preview=1,wait=0.4,xattach=https://studio.x.com/live/QUIETCHAT01,wait=6.5,expect=xCrop:shown,wait=0.8,expect=xRegion:363x140,expect=xPixels:ff00ff=none,expect=xPixels:123456=none,wait=1.2,expect=xFresh:3", fake_backend=True)   # a quiet chat page keeps being asked for fresh frames, so a stale black one cannot stay
    # The livestream that is on air is found by MOBS itself: no link to copy.
    check("the livestream that is Live Now in Live Studio is found by itself and its chat attached",
          live_setup + "xurl=https://x.com/home,pick=2,preview=1,wait=0.4,arm=1,start,expect=state:recording,wait=13,expect=chatXId:1PJqrNmYLjYxb,wait=4,expect=xCrop:shown,expect=xPixels:ff00ff=none", fake_backend=True, limit=40)
    check("a livestream that was already on air when the broadcast began is never taken for this one",
          live_setup + "xurl=https://x.com/home,pick=2,preview=1,wait=0.4,arm=1,start,expect=state:recording,wait=14,expect=chatXId:,expect=chatWanted:0", fake_backend=True, limit=40,
          extra_env={"MOBS_X_STUDIO_URL": (Path(__file__).resolve().parent / "fake-studio-stale.html").as_uri()})
    check("two livestreams that go on air at once are not guessed between: the person is asked to paste the right link",
          live_setup + "xurl=https://x.com/home,pick=2,preview=1,wait=0.4,arm=1,start,expect=state:recording,wait=14,expect=chatXId:,expect=chatWanted:0,expect=finderNote:Two livestreams", fake_backend=True, limit=40,
          extra_env={"MOBS_X_STUDIO_URL": (Path(__file__).resolve().parent / "fake-studio-two.html").as_uri()})
    check("a livestream link that was already on the clipboard when the broadcast began is not taken for it",
          live_setup + "clip=https://studio.x.com/live/1OLDoldOLDold9,xurl=https://x.com/home,pick=2,preview=1,wait=0.4,arm=1,start,expect=state:recording,wait=5,expect=mainHeld:1,expect=chatXId:,expect=chatWanted:0", fake_backend=True, limit=30,
          extra_env={"MOBS_X_STUDIO_URL": (Path(__file__).resolve().parent / "fake-studio-quiet.html").as_uri()})
    check("a new livestream is found even when it only shows after the page is reloaded",
          live_setup + "xurl=https://x.com/home,pick=2,preview=1,wait=0.4,arm=1,start,expect=state:recording,wait=19,expect=chatXId:1PJqrNmYLjYxb", fake_backend=True, limit=45,
          extra_env={"MOBS_X_STUDIO_URL": (Path(__file__).resolve().parent / "fake-studio-reload.html").as_uri()})
    check("stopping a broadcast whose livestream was never found opens the list of livestreams",
          live_setup + "xurl=https://x.com/home,pick=2,preview=1,wait=0.4,arm=1,start,expect=state:recording,wait=1,stop,waitidle", fake_backend=True,
          expect_log=("opening https://studio.x.com/live",), forbid_log=("opening https://studio.x.com/live/",),
          extra_env={"MOBS_X_STUDIO_URL": (Path(__file__).resolve().parent / "fake-studio-quiet.html").as_uri()})
    check("a broadcast X never received leaves nothing open: stopping it says nothing and opens no browser",
          live_setup + "xurl=https://x.com/home,xattach=https://studio.x.com/live/1PJqrNmYLjYxb,pick=2,preview=1,wait=0.4,arm=1,start,expect=state:recording,wait=1,expect=streamState:connecting,stop,waitidle", fake_backend=True,
          expect_log=("ended without X ever receiving the stream",), forbid_log=("opening https://studio.x.com/live",),
          extra_env={"MOBS_FAKE_STREAM_NEVER_LIVE": "1"})
    check("when nothing is on air in Live Studio no chat is attached",
          live_setup + "xurl=https://x.com/home,pick=2,preview=1,wait=0.4,arm=1,start,expect=state:recording,wait=9,expect=chatXId:,expect=chatWanted:0", fake_backend=True,
          extra_env={"MOBS_X_STUDIO_URL": (Path(__file__).resolve().parent / "fake-studio-quiet.html").as_uri()})
    check("stopping a take whose livestream is known says it is still open on X and opens it in Live Studio",
          live_setup + "xurl=https://x.com/home,xattach=https://studio.x.com/live/1PJqrNmYLjYxb,pick=2,preview=1,wait=0.4,arm=1,start,expect=state:recording,wait=1,stop,waitidle", fake_backend=True,
          expect_log=("opening https://studio.x.com/live/1PJqrNmYLjYxb",))
    check("stopping a plain recording opens nothing", "pick=2,preview=1,wait=0.4,xattach=https://studio.x.com/live/1PJqrNmYLjYxb,start,expect=state:recording,wait=1,stop,waitidle", fake_backend=True,
          forbid_log=("opening https://studio.x.com/live/",))
    check("a signed-out person's broadcast is not searched for a livestream",
          live_setup + "pick=2,preview=1,wait=0.4,arm=1,start,expect=state:recording,wait=9,expect=chatXId:", fake_backend=True)
    check("a chat card that X answers with a sign-in page says so",
          "pick=2,preview=1,wait=0.4,xattach=https://studio.x.com/live/SIGNEDOUT01,expect=chatXId:SIGNEDOUT01,wait=7,expect=xCrop:signedout,expect=xFresh:none", fake_backend=True, extra_env={"MOBS_X_TRIES": "2"})
    check("a livestream page with no chat card says so",
          "pick=2,preview=1,wait=0.4,xattach=https://studio.x.com/live/NOCHAT000001,expect=chatXId:NOCHAT000001,wait=7,expect=xCrop:missing,expect=xFresh:none", fake_backend=True, extra_env={"MOBS_X_TRIES": "2"})
    check("a chat card whose page has no chat card asks X's page again now and then, so a sign-in or the network coming back is noticed",
          "pick=2,preview=1,wait=0.4,xattach=https://studio.x.com/live/SIGNEDOUT01,expect=chatXId:SIGNEDOUT01,wait=19,expect=xCrop:signedout", fake_backend=True, extra_env={"MOBS_X_TRIES": "2"},
          expect_log=("asking X's page again",), limit=45)
    check("with the usual patience the chat card first says to sign in, and only then asks X's page again (a reload resets the count, so one that came first would keep the verdict from ever being given)",
          "pick=2,preview=1,wait=0.4,xattach=https://studio.x.com/live/SIGNEDOUT01,expect=chatXId:SIGNEDOUT01,wait=25,expect=xCrop:signedout", fake_backend=True,
          expect_log=("asking X's page again",), limit=55)
    check("a link that is not a Live Studio link attaches nothing", "pick=2,preview=1,wait=0.4,xattach=https://example.com/i/live-studio/1PJqrNXnQywxb,expect=chatXId:,expect=chatWanted:0", fake_backend=True)
    check("the chat of a livestream ends with the take", "pick=2,preview=1,wait=0.4,xattach=https://x.com/i/broadcasts/1PJqrNXnQywxb,expect=chatXId:1PJqrNXnQywxb,start,expect=state:recording,stop,waitidle,expect=chatXId:,expect=chatWanted:0", fake_backend=True)
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
        check("the setup guide fits on every page", "wait=0.4," + ",".join(f"setupstep={n},wait=0.35,checklayout" for n in range(8)), size, True, extra_env=setup_env)
    check("the webcam window is off when the setting is off, and the picture has no drag handle then either way", "pick=2,preview=1,wait=0.4,expect=pipWanted:0,expect=pipShown:0", fake_backend=True, ini_extra="pip=false\n")
    check("my screen and just my camera have no webcam window", "pick=1,preview=1,wait=0.3,expect=pipShown:0,pick=3,wait=0.3,expect=pipShown:0", fake_backend=True)
    check("the webcam window toggle in EXPERT", "pro=1,tab=0,wait=0.1,checklayout", "860x560")
    check("there is no webcam to drag in the other picks", "pick=1,wait=0.3,expect=camMoved:0,checklayout", fake_backend=True)
    # Going live (the fake backend pretends to connect, go live, drop and come back). The server address and key here are made up.
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
        check("live setup page with a saved key fits", live_setup + "pro=1,tab=6,expect=streamReady:1,wait=0.1,checklayout", size)
        check("ready screen armed to go live fits", live_setup + "pick=1,arm=1,expect=armed:1,wait=0.1,checklayout", size, True)
        check("ready screen armed with the preview fits", live_setup + "pick=1,preview=1,arm=1,expect=previewFrame:1,wait=0.1,checklayout", size, True)
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "900x1600", "1206x2000"):
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
    # The go-live helper (LivePilot) does the steps in Live Studio for the person, in MOBS's own browser. A stand-in page with the structure of the real one (fake-livestudio.html,
    # read from the real page on 2026-10-13) stands in for X: the Create Livestream form, its Title, Source and Audience steps, Go Live grey until there is a title and the source is on.
    def studio(query=""):
        return {"MOBS_NO_PILOT": "0", "MOBS_PILOT_TICK_MS": "250", "MOBS_X_STUDIO_URL": (Path(__file__).resolve().parent / "fake-livestudio.html").as_uri() + ("?" + query if query else "")}
    pilot_start = live_setup + "xurl=https://x.com/home,pick=1,arm=1,start,expect=state:recording,"
    check("the helper types the title, waits until X sees the video, presses Go Live, attaches the chat and the window leaves by itself",
          pilot_start + "livetitle=My first show,wait=0.5,expect=mainHeld:1,expect=chatXId:,wait=8,expect=pilotPhase:none,expect=chatXId:1PJqrNmYLjYxb,wait=2.2,expect=mainHeld:0",
          fake_backend=True, extra_env=studio("on=2500"), limit=45,
          expect_log=("[pilot] filling:", "typing the title (13 characters)", "who can watch: public", "[pilot] waiting: Waiting for X to see your video", "[pilot] pressing: X sees your video", "[pilot] done: You're live", "X chat attached"),
          forbid_log=("My first show",))   # the title is the person's own words: it is typed into X, and the log only says how long it was
    check("Go Live stays grey until the title is typed: the title Live Studio showed is replaced by the one chosen here",
          pilot_start + "livetitle=New title,wait=4,expect=pilotTitle:New title,expect=pilotPhase:waiting,expect=pilotReason:Your source must be on to go live,expect=pilotProgress:2,expect=chatXId:",
          fake_backend=True, extra_env=studio("on=60000&prefill=Old title"), limit=30)
    check("with no title chosen the helper types the default one, so Go Live is never grey for want of a title",
          pilot_start + "wait=4,expect=pilotTitle:Live with MOBS,expect=liveTitle:Live with MOBS,expect=pilotPhase:waiting", fake_backend=True, extra_env=studio("on=60000"), limit=30)
    check("a livestream for me only is set to Private in the form", pilot_start + "livepublic=0,wait=5,expect=pilotVisibility:private,expect=pilotPhase:waiting", fake_backend=True, extra_env=studio("on=60000"), limit=30)
    check("a form that starts on Private is changed to Public when everyone should watch", pilot_start + "wait=5,expect=pilotVisibility:public,expect=pilotPhase:waiting", fake_backend=True, extra_env=studio("on=60000&private=1"), limit=30)
    check("with two sources the one the person named is picked, and the livestream goes live",
          pilot_start + "sourcename=MOBS,wait=9,expect=chatXId:1PJqrNmYLjYxb", fake_backend=True, extra_env=studio("two=1&on=300"), limit=45)
    check("a source that is never seen on makes the helper say so in X's words and hand over", 
          pilot_start + "wait=6,expect=pilotPhase:failed,expect=pilotFailure:still does not see your video,click=goLiveHide,expect=mainHeld:0",
          fake_backend=True, extra_env=studio("never=1") | {"MOBS_PILOT_WAIT_MS": "2500"}, limit=30)
    check("an error from Live Studio after Go Live is shown and the helper hands over", pilot_start + "wait=7,expect=pilotPhase:failed,expect=pilotFailure:Error creating livestream", fake_backend=True, extra_env=studio("on=300&fail=1"), limit=30)
    check("a signed-out Live Studio is signed in to in the box, and the helper carries on by itself afterwards",
          pilot_start + "wait=1.5,expect=pilotPhase:signin,wait=9,expect=chatXId:1PJqrNmYLjYxb", fake_backend=True, extra_env=studio("login=3000&on=300"), limit=45)
    for size, px in (("1180x720", 15), ("1500x940", 19)):
        check(f"no text under {px} pixels in the go-live window, working and given up ({size})",
              pilot_start + f"wait=3,expect=pilotPhase:waiting,checktext={px},wait=5,expect=pilotPhase:failed,checktext={px}", size, True, extra_env=studio("never=1") | {"MOBS_PILOT_WAIT_MS": "2500"}, limit=40)
        check(f"no text under {px} pixels on the recording screen and the ready screen with everything on ({size})",
              live_setup + f"pick=1,preview=1,wait=0.3,arm=1,checktext={px},start,expect=state:recording,wait=0.6,checktext={px}", size, True, limit=30)
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
        check("the go-live window with the helper at work fits", pilot_start + "wait=3,expect=pilotPhase:waiting,checklayout", size, True, extra_env=studio("on=60000"), limit=30)
        check("the go-live window with the helper given up fits", pilot_start + "wait=5,expect=pilotPhase:failed,checklayout", size, True, extra_env=studio("never=1") | {"MOBS_PILOT_WAIT_MS": "2500"}, limit=30)
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
        check("the ready screen with the title box and who can watch fits", live_setup + "pick=1,wait=0.3,expect=readyTitle:A rather long title for my livestream about omarchy,checklayout,livepublic=0,wait=0.1,checklayout", size, True,
              ini_extra="liveTitle=A rather long title for my livestream about omarchy\n")
    check("a copied X link that is not a livestream link gets a note, and a real one attaches the chat",
          live_setup + "xurl=https://x.com/home,pick=1,arm=1,start,expect=state:recording,clip=https://x.com/NixFred/status/1234567890123456789,wait=3,expect=mainHeld:1,expect=goLiveNote:not a livestream link,checklayout,"   # copied DURING the broadcast: what is on the clipboard when it begins is old
          "clip=https://x.com/i/broadcasts/1PJqrNXnQywxb,wait=3,expect=chatXId:1PJqrNXnQywxb,expect=goLiveNote:Chat attached", fake_backend=True,
          extra_env={"MOBS_X_STUDIO_URL": (Path(__file__).resolve().parent / "fake-studio-quiet.html").as_uri()})
    check("stopping the stream ends the hold", live_setup + "pick=1,arm=1,start,expect=state:recording,wait=0.5,expect=mainHeld:1,click=goLiveStop,waitidle,expect=mainHeld:0", fake_backend=True)
    check("arming without an address or key sends the user to set them up",
          "pick=1,arm=1,expect=armed:1,start,expect=state:idle,expect=message:go live needs,expect=setupOpen:1,expect=setupStep:5,wait=0.1,checklayout", fake_backend=True)
    check("the hotkey chip arms and disarms once a key is saved",
          live_setup + "pick=1,click=liveChip,expect=armed:1,click=liveChip,expect=armed:0", fake_backend=True)
    # The picture is on by default, and the broadcast has its own button beside Record.
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
        check("the ready screen shows the picture by default and fits with the GO LIVE button", "pick=2,wait=0.4,expect=preview:1,expect=previewing:1,expect=previewFrame:1,checklayout", size, True, default_preview=True)
        check("the ready screen with the picture, a saved key and the GO LIVE button fits", live_setup + "pick=2,wait=0.4,expect=previewFrame:1,checklayout", size, True, default_preview=True)
    check("the GO LIVE button arms and starts a broadcast in one click", live_setup + "pick=1,click=goLiveButton,expect=state:recording,expect=broadcasting:1,expect=armed:0,wait=0.8,expect=streamState:live", fake_backend=True)
    check("the GO LIVE button without a saved address and key opens the guide at the X page", "pick=1,click=goLiveButton,expect=state:idle,expect=setupOpen:1,expect=setupStep:5,wait=0.1,checklayout", fake_backend=True)
    check("arming to go live always shows the picture, even with the preview chip off", "pick=1,expect=previewing:0,arm=1,expect=previewing:1,expect=previewFrame:1,arm=0,expect=previewing:0", fake_backend=True)
    check("the close EXPERT button on the live page leaves EXPERT", live_setup + "pro=1,tab=6,wait=0.1,click=closeLive,expect=pro:0", fake_backend=True)
    check("a take that is not live shows no broadcast", "pick=1,start,expect=state:recording,wait=0.6,expect=broadcasting:0,expect=streamState:off,checklayout", fake_backend=True)
    # A screen share that delivers nothing but black (a dragged Region does this on gus) is named, and it never goes live.
    black = {"MOBS_BLACK_AFTER_MS": "300"}
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
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
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
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
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
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
    for size in ("860x560", "1024x768", "1180x720", "1500x940", "2560x1000"):
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
