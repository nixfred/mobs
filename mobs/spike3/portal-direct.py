# Direct ScreenCast portal test, no OBS: CreateSession -> SelectSources -> Start (this is the step that shows the picker).
import sys, time
import gi
from gi.repository import Gio, GLib

bus = Gio.bus_get_sync(Gio.BusType.SESSION)
me = bus.get_unique_name()[1:].replace(".", "_")
loop = GLib.MainLoop()
t0 = time.time()
def log(s): print(f"[{time.time()-t0:5.1f}s] {s}", flush=True)

def call(method, params, token, timeout_s):
    """Call a portal method and wait for its Response signal. Returns (code, results)."""
    path = f"/org/freedesktop/portal/desktop/request/{me}/{token}"
    out = {}
    def on_resp(conn, sender, p, iface, sig, args, *_):
        out["code"], out["res"] = args[0], args[1]
        loop.quit()
    sid = bus.signal_subscribe("org.freedesktop.portal.Desktop", "org.freedesktop.portal.Request", "Response", path, None, 0, on_resp)
    def on_timeout():
        out["timeout"] = True
        loop.quit()
        return False
    tid = GLib.timeout_add_seconds(timeout_s, on_timeout)
    bus.call_sync("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.ScreenCast", method, params, None, Gio.DBusCallFlags.NONE, -1)
    loop.run()
    GLib.source_remove(tid) if not out.get("timeout") else None   # no stale timer may cut a later wait short
    bus.signal_unsubscribe(sid)
    return out

props = Gio.DBusProxy.new_sync(bus, 0, None, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.DBus.Properties", None)
v = props.call_sync("Get", GLib.Variant("(ss)", ("org.freedesktop.portal.ScreenCast", "AvailableSourceTypes")), 0, -1)
log(f"AvailableSourceTypes = {v.unpack()[0]} (1 monitor, 2 window)")

r = call("CreateSession", GLib.Variant("(a{sv})", ({"handle_token": GLib.Variant("s", "mobs1"), "session_handle_token": GLib.Variant("s", "mobsses")},)), "mobs1", 15)
log(f"CreateSession -> {r}")
if "res" not in r: sys.exit(2)
session = r["res"]["session_handle"]
import os
PEEK = os.environ.get("PEEK") == "1"
r = call("SelectSources", GLib.Variant("(oa{sv})", (session, {"handle_token": GLib.Variant("s", "mobs2"), "types": GLib.Variant("u", 1), "multiple": GLib.Variant("b", False), "cursor_mode": GLib.Variant("u", 1), "persist_mode": GLib.Variant("u", 2)})), "mobs2", 7 if PEEK else 90)
log(f"SelectSources (picker is shown during this call) -> {r}")
if PEEK:
    # cancel cleanly so xdph is not left with an abandoned request
    try:
        Gio.DBusProxy.new_sync(bus, 0, None, "org.freedesktop.portal.Desktop", session, "org.freedesktop.portal.Session", None).call_sync("Close", None, 0, 5000)
        log("session closed cleanly")
    except Exception as e:
        log(f"(session close: {e})")
    sys.exit(0)
log("Start: THE PICKER SHOULD APPEAR NOW. waiting up to 60 s for an answer...")
r = call("Start", GLib.Variant("(osa{sv})", (session, "", {"handle_token": GLib.Variant("s", "mobs3")})), "mobs3", 60)
log(f"Start -> {r}")
try:
    Gio.DBusProxy.new_sync(bus, 0, None, "org.freedesktop.portal.Desktop", session, "org.freedesktop.portal.Session", None).call_sync("Close", None, 0, 2000)
except Exception as e:
    log(f"(session close: {e})")
