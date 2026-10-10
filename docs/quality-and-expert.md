# MOBS usability and reliability pass — 2026-10-10

MOBS starts with three choices and one Record button. Keep that path short. Put specialized controls behind a clearly labelled **EXPERT** button, and describe each choice in terms of the recording it produces.

## Delivered in this pass

- The ready screen has a mic toggle, a truthful mic status, and a quality chip that opens EXPERT. Balanced remains the default.
- EXPERT uses the full content area with recording, encoding, audio, keys, about and hardware pages. Closing it restores the original screen. It retains visible pause and stop controls during recording.
- Encoding exposes x264 CRF 0–51 and nine CPU presets. Plain quality presets clear a custom CRF; reset encoding restores the selected quality's CRF and the veryfast CPU preset. Settings persist and apply to the next take. The output remains 1080p/30 fps, H.264/AAC MP4.
- Folder changes require an explicit **use folder** action. Invalid paths retain the saved destination and show an explanation. Typing a path cannot accidentally trigger recording shortcuts; Esc restores keyboard focus.
- Camera and EXPERT panels block the underlying screen's shortcuts. The camera panel fits the minimum window with the two-device fixture.
- Ready names the chosen microphone and offers independent mic and desktop-sound toggles. EXPERT audio lists named inputs and output monitors with readable pagination. An explicit mic check captures without saving or playing sound; live meters report levels and missing frames. Independent gains (−60 to +12 dB) live behind **EXPERT gain**. Microphone defaults to 0 dB; desktop audio defaults off at −6 dB. Both sources mix into one AAC track.
- EXPERT hardware checks NVIDIA H.264 availability without opening capture devices. Software remains the default. NVIDIA encoding uses CQP quality and the p5 preset; CPU encoding retains its custom CRF and preset. The system NVIDIA test helper is copied and installed alongside MOBS when available.
- Audio discovery runs asynchronously with a timeout. An unavailable input explains why recording is blocked; turning both audio sources off permits silent recording. Saved malformed gains fall back to defaults.

## Bugs repaired

1. Recording filenames included only seconds, allowing quick takes to reuse a name. Names now include milliseconds and skip existing takes and pause sidecars.
2. A global recording request could close an editor with unsaved work. Every start path now checks for unsaved edits or an active export and keeps the editor open with an explanation.
3. Closing the window could interrupt a recording or discard edits. Close now stops and saves an active recording before exiting. Unsaved edits require a choice; active exports must finish first. A failed or cancelled recording keeps its explanation visible.
4. Export used POSIX rename, which could overwrite a destination created after the export started. Finalization now refuses an existing destination, removes the partial, and preserves unsaved status. A real ffmpeg regression covers this race.
5. Recorder polling handled only user-requested stops. A disk-full or encoder failure could leave the interface reporting recording indefinitely. The libobs stop signal now feeds the Qt-thread cleanup and reports unsuccessful termination. The simulated disk-full test failed before the fix and passes afterward.
6. Mic and save-folder labels used hard-coded values. Ready follows saved settings; recording follows the settings captured when that take began.
7. A preview failure used the recording failure signal. Preview failures now clear preview state through a separate signal and leave an active recording alone.
8. Failed or unexpectedly finished takes lost the current stretch of elapsed time. The clock now banks it before returning to idle.
9. The development script's asynchronous wait captured a local variable by reference after its scope ended. The runner now keeps the continuation alive safely and releases its callbacks at shutdown.
10. Another file could appear while the screen picker was open and then be truncated when capture finally started. The backend now claims the recording path with atomic create-only semantics before starting output. A native regression proves the intervening file stays byte-for-byte intact. Filename selection also skips dangling symlinks.
11. Splitting unchanged content, or splitting an already saved edit, falsely marked it unsaved. Dirty state now compares kept content rather than clip segmentation. Repeated saves of an unchanged edit no longer create duplicate exports.
12. A completed trim did not always notify the UI that undo was available. Gesture completion now reliably publishes edit state; non-finite trim or split values are rejected before clips can disappear.
13. Monologue's unmodified timeline called two missing host functions: precise time formatting during a drag and gap restoration. MOBS now supplies both. Cancelled drags, hidden review screens and window deactivation finish the gesture, leaving one undo step.
14. A recording request blocked by unsaved edits left an obsolete warning after saving or changing the edit. Review updates now clear that idle warning while preserving active recording messages.
15. At 860×560 a missing-input explanation could push the large Record circle above its content area. The minimum-window circle and spacing now fit, including preview and meter states. Layout checks cover the circle and preview geometry as well as text, and capture QML runtime warnings directly.
16. Grok's independent review found the live-preview caption extended outside its component and underneath Record once a frame arrived. Preview now reserves a caption row; Ready budgets that space. The script backend now supplies generated preview frames, and layout validation checks caption/button intersection.
17. Grok also found a stale mic-check failure could hide a newer recording error and disappear after unrelated settings changes. Ready now shows both explanations without duplicate text. Mic checks restart only for mic changes; unrelated settings preserve failures. Turning the mic off, explicitly stopping/starting a check, or selecting an available new input clears the old error.
18. A recording stop that never completed trapped every window-close attempt. The first close still requests a normal save. Closing again while saving offers **keep waiting** or **quit without waiting**, with an explicit warning that this take may be incomplete or unplayable. The confirmed quit bypasses potentially stuck encoder teardown; it is available only while stopping. A fake stalled backend covers cancellation, keyboard protection and actual process exit. No real encoder hang was reproduced.
19. Opening a recording synchronously waited for ffprobe on the UI thread. Review inspection now runs asynchronously with a 30-second timeout. Ready displays loading status, new recordings wait for inspection, and closing cancels it. Cancelled probes are destroyed after their exit notification, avoiding a blocking process-destructor warning. Finishing or cancelling inspection clears its obsolete blocked-recording notice. A delayed-probe regression failed before the fix; cancellation, immediate recording after cancellation, and missing-ffprobe recovery also pass, with the cancellation warning treated as a failure.
20. Failed review inspection stored an explanation on the review screen that never opened. Its failure now appears in Ready's notice. A garbage-file UI regression reproduced the invisible error before the fix.
21. Preview failure stopped frame delivery but left the persisted preference and chip saying **preview on**. Failure now switches that preference off, leaving the explanation visible and permitting a one-click retry. The fake-preview regression failed before the fix.
22. A picture-check process that failed to launch remained attached to Engine until exit. Failed launch now schedules cleanup. A temporary executable with a missing interpreter reproduces the failure and verifies cleanup.
23. Review prioritized a black-picture warning over export results, hiding a failed save. The review notice now shows distinct picture warnings, recording messages and export messages together. A generated black recording and deliberately failing export reproduced the hidden error before the fix.
24. Review's action row was wider than its narrow video column but shared the column's left edge. Expanded leave/trash confirmations pushed **keep it** off-screen at 860×560. Controls now have a centered width of their own; preview size uses the actual space required by controls and notices. All three tested window sizes cover the expanded confirmations.
25. Camera selection added an unbounded row per device. Six simulated cameras pushed the title, choices and Done off-screen. Camera selection now reuses the compact paged device chooser, with no scrolling and readable names. Tests reach and select the sixth camera at all three window sizes. The chooser also guards a missing current item while its list changes.

## Independent Grok review

The installed Grok CLI performed a read-only review of the working tree at Fred's request. It returned two medium-severity findings (items 16–17 above). Both were independently reproduced with failing UI regressions before the fixes. The audio finding has separate regressions for an obscured save-folder failure and an unrelated settings change clearing the error. Grok also checked the actual PulseAudio device schema, NVIDIA encoder settings, gain behavior and normal MP4 stop signals; it did not run recording tests. The project's own integration checks provide that coverage.

## Independent Kimi3 review

At Fred's request, the installed Kimi CLI reviewed the working tree read-only using `kimi-code/k3`. It returned six findings. Five reliability issues are addressed in items 18–22. Four were reproduced with failing regressions; the never-completing-stop escape is tested with a deliberately stalled fake backend. Kimi's suggestion of an automatic forced teardown was not adopted: quitting early requires an explicit user choice.

The remaining finding is a behavior ambiguity: pressing X exactly at the last kept clip's end restores a trimmed tail. Clip ranges exclude their end, so that position belongs to the gap; the existing cut/restore command restores gaps and supports undo. This behavior is retained. A clearer distinction between cutting a selected clip and restoring a gap remains a usability follow-up. Kimi's proposed NVENC/PipeWire causes of a stalled stop were hypotheses; the separately documented NVIDIA unload/reload crash does not establish a recording-stop hang.

## How EXPERT should grow

| Feature | Everyday controls | EXPERT controls |
| --- | --- | --- |
| Microphone | Implemented: on/off, named input, explicit check and meter | Implemented: device and gain. Later: noise suppression, monitoring |
| Desktop audio | Implemented: off/on and recording meter | Implemented: output and gain. Later: routing, track assignment |
| Picture | Implemented: small / balanced / high | Implemented: software/NVIDIA encoding, CRF/CQP, CPU preset. Later: resolution, fps, keyframes |
| Camera | Device, corner, small / medium / large | Format, crop, transform, filters |
| Editor | Play, trim handles, save, done | Visible split/cut, undo/redo, precise times, export settings |
| Scenes / streaming — later | Named presets and a guided connection | Sources, transitions, service settings and output diagnostics |

Add each EXPERT entry beside the feature it configures as those features arrive. Do not add switches for settings the backend does not yet implement. Keep a clear live status, a route back to the simple view, and a reset for advanced overrides. Growing pages should use groups or pagination with readable text; avoid a growing scroll-only settings drawer.

## Validation and limits

The final build succeeded. All **12 CTest suites** passed after the continued bug hunt, including **72 UI scenarios**. Two native application checks were repeated with generated video and microphone/desktop capture disabled: stopping while paused opened review, and closing during recording saved and exited. Both produced playable H.264 files with decoded silent audio.

The [stalled-save dialog](app/stalled-save.png), [editor confirmations](app/review-confirmations.png), [visible export failure beside a picture warning](app/review-save-error.png) and [six-camera chooser](app/camera-many.png) were rendered and visually inspected at 860×560. The new failures were reproduced before their fixes; the UI checks cover expanded confirmations and camera selection at all three tested sizes.

The preceding separate Wayland run passed **22 isolated libobs scenarios**, with no skips; the Kimi and later UI passes did not change ObsBackend or rerun those 22 scenarios. The ordinary CTest run intentionally skips real-libobs initialization; the native application checks above exercise the save/review/close paths.

The minimum-size [ready screen](app/ready-simple.png), [EXPERT encoding](app/expert-encoding.png), [audio inputs](app/expert-audio.png), [gain controls](app/expert-audio-gain.png) and [ready with audio meters](app/ready-audio.png) were rendered and visually inspected at 860×560. Automated visible bounds passed at all three tested sizes. This is offscreen application rendering, not live desktop screen-fit verification.

The caption fix was also checked with actual libobs-generated frames in a native Wayland window, with mic off. The compositor assigned 1180×967 despite the requested sizes; this verifies native frame delivery and caption fit at that size. The offscreen tests supply the 860×560, 1024×768 and 1180×720 coverage.

The project builds with its existing system libobs dependency and uses `pactl` for audio discovery. Unit tests cover settings persistence, invalid values, device parsing and discovery failure, filename collisions, guarded starts, recording snapshots, preview failure, editor gestures and export collisions. The UI suite runs the actual QML with temporary settings, fake cameras/audio and generated video at 860×560, 1024×768 and 1180×720; it checks all six EXPERT pages, keyboard focus, modal shortcuts, error states, editor protection, real pointer trim/cancellation and visible bounds. Gap restoration is tested through the QML host callback; a physical double-click is not established by that check.

Wayland integration uses generated video and sound, without recording real screens, cameras or microphones. It tests playable recording, pause, repeated takes, camera geometry, preview lifetime, quality, custom encoding, output failure, cancellation, delayed filename collisions, mic checks, measured saved gain, desktop-only sound, mixing and NVIDIA/software switching. The available NVIDIA encoder produced playable files on this host. Physical audio devices, hotplug and the real screen picker on both machines still need acceptance checks. Resolution/fps, audio filters, monitoring and separate tracks remain future work. These checks cannot establish that the whole application is bug-free.

The installed NVIDIA OBS plugin aborted during repeated libobs unload/reload inside one integration-test process. Crash analysis located `obs-nvenc.so` teardown, with retained CUDA state in the corresponding source. Native scenarios now run in separate processes, matching the application's single libobs lifetime; repeated takes and NVIDIA/software switching remain tested inside one backend. No system plugin or upstream subtree was changed.

Qt's audio initialization was blocked intermittently by the command sandbox. Final GUI and integration checks therefore run outside it with isolated settings and stand-in media. No real screen, camera or microphone is recorded. The existing installed application and desktop bindings are not changed by this pass.
