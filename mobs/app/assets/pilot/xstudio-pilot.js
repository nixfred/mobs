// The page half of MOBS's helper for X Live Studio. MOBS loads this into the Live Studio page (studio.x.com) in its own browser, signed in as the person, and then asks it small
// questions and gives it one small job at a time: look (what is on the page), openNew, setTitle, tab, setVisibility, chooseSource, goLive, closeDialog and, on the Sources page,
// openKey, readKey and createSource. Every call answers a JSON string. Nothing here is a guess about X's markup: the names, labels and sentences below were read from the real page
// (the Create Livestream dialog, 2026-10-13) and are matched by their words, never by X's generated class names.
// The stream key is the one secret on these pages: readKey hands it back to MOBS, which keeps it in a private file; nothing in this file writes it anywhere else.
(function () {
  if (window.__mobsPilot && window.__mobsPilot.version === 1) return 'ready';

  var D = document;
  function all(selector, root) { return Array.prototype.slice.call((root || D).querySelectorAll(selector)); }
  function words(e) { return ((e && (e.innerText || e.textContent)) || '').replace(/\s+/g, ' ').trim(); }
  function shown(e) {
    if (!e || !e.getBoundingClientRect) return false;
    var r = e.getBoundingClientRect();
    if (r.width <= 0 || r.height <= 0) return false;
    var s = getComputedStyle(e);
    return s.visibility !== 'hidden' && s.display !== 'none';
  }
  function off(button) { return !!(button && (button.disabled || button.getAttribute('aria-disabled') === 'true')); }
  // A click the way a hand makes it (X's menus and radio buttons listen for the pointer events, its plain buttons only for the click).
  function press(e) {
    ['pointerdown', 'mousedown', 'pointerup', 'mouseup', 'click'].forEach(function (type) {
      var Kind = type.indexOf('pointer') === 0 && window.PointerEvent ? PointerEvent : MouseEvent;
      e.dispatchEvent(new Kind(type, { bubbles: true, cancelable: true, view: window, button: 0, buttons: type === 'pointerdown' || type === 'mousedown' ? 1 : 0, pointerId: 1, isPrimary: true }));
    });
  }
  // React keeps its own copy of an input's value: the page only hears about a new one through the browser's own setter and an input event.
  function setValue(input, value) {
    var proto = input.tagName === 'TEXTAREA' ? HTMLTextAreaElement.prototype : HTMLInputElement.prototype;
    var setter = Object.getOwnPropertyDescriptor(proto, 'value').set;
    input.focus();
    setter.call(input, value);
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.dispatchEvent(new Event('change', { bubbles: true }));
  }
  function buttonWithWords(pattern, root) {
    return all('button, [role=button]', root).filter(function (b) { return shown(b) && pattern.test(words(b) || b.getAttribute('aria-label') || ''); })[0] || null;
  }
  function dialogs() { return all('[role=dialog]').filter(shown); }
  function createDialog() {
    return dialogs().filter(function (d) { return /create livestream/i.test(d.getAttribute('aria-label') || words(d).slice(0, 80)); })[0] || null;
  }
  // The colour of a small round "status dot": green means the source is receiving a stream (X draws it emerald, grey otherwise).
  function dotIsGreen(root) {
    var found = null;
    all('*', root).some(function (e) {
      var r = e.getBoundingClientRect();
      if (r.width < 4 || r.width > 14 || Math.abs(r.width - r.height) > 1) return false;
      var cs = getComputedStyle(e);
      if (parseFloat(cs.borderRadius) < r.width / 2 - 1 && cs.borderRadius.indexOf('%') < 0) return false;
      try {
        var c = D.createElement('canvas'); c.width = c.height = 1;
        var g = c.getContext('2d', { willReadFrequently: true }); g.fillStyle = cs.backgroundColor; g.fillRect(0, 0, 1, 1);
        var px = g.getImageData(0, 0, 1, 1).data;
        found = px[3] > 100 && px[1] - px[0] > 40 && px[1] - px[2] > 20;
      } catch (error) { found = null; }
      return true;
    });
    return found;
  }
  // The sentence X puts beside Go Live while it is disabled ("Add a title to go live.", "Your source must be on to go live."): it is X's own reason, shown to the person as it is.
  // The button points at it (aria-describedby); failing that it is the text after the last "Go Live" of the form, which is where X writes it.
  function reasonOf(dialog, go) {
    var ids = go ? (go.getAttribute('aria-describedby') || '').split(/\s+/).filter(Boolean) : [];
    var named = ids.map(function (id) { var e = D.getElementById(id); return e ? words(e) : ''; }).filter(function (t) { return t; })[0];
    if (named) return named.slice(0, 140);
    var parts = words(dialog).split(/Go Live/);
    var tail = parts.length > 1 ? parts[parts.length - 1].trim() : '';
    return tail.length > 0 && tail.length <= 140 ? tail : '';
  }
  function tabOf(dialog) {
    var t = all('button[role=tab]', dialog).filter(function (b) { return b.getAttribute('aria-selected') === 'true'; })[0];
    return t ? words(t).toLowerCase() : '';
  }
  // A livestream's page is /live/<id>. An id has letters AND digits (the page "/live/settings" is not a livestream).
  function livestreamIdFromAddress() {
    var m = /(?:^|\/)live\/([A-Za-z0-9]{8,24})(?:[\/?#]|$)/.exec(location.pathname + location.hash);
    return m && /\d/.test(m[1]) && /[A-Za-z]/.test(m[1]) ? m[1] : '';
  }
  function toastText() {
    var t = all('[role=alert], [role=status], [data-sonner-toast], [class*=toast i]').filter(shown).map(words).filter(function (x) { return x; })[0];
    return t ? t.slice(0, 160) : '';
  }
  function say(o) { return JSON.stringify(o); }

  var api = {
    version: 1,

    // What is on the page right now. Cheap, changes nothing.
    look: function () {
      var out = { path: location.pathname + location.hash, host: location.host, complete: D.readyState === 'complete', live: livestreamIdFromAddress(), toast: toastText() };
      var bodyText = words(D.body);
      out.signedOut = /^\/(login|i\/flow\/login|i\/flow\/signup)/.test(location.pathname) || (all('input[name=password], input[autocomplete="current-password"]').some(shown) && /sign in|log in/i.test(bodyText.slice(0, 600)));
      var newButton = buttonWithWords(/^new livestream$/i);
      out.newButton = newButton ? (off(newButton) ? 'off' : 'on') : '';
      out.emptyList = /no livestreams yet/i.test(bodyText);
      var dlg = createDialog();
      out.dialog = !!dlg;
      if (dlg) {
        var title = dlg.querySelector('#broadcast-title') || dlg.querySelector('input[placeholder^="Enter a title"]');
        var combo = dlg.querySelector('button[role=combobox]');
        var go = buttonWithWords(/^go live$/i, dlg);
        out.tab = tabOf(dlg);
        out.title = title ? title.value : null;
        out.source = combo ? words(combo) : '';
        out.sourceGreen = combo ? dotIsGreen(combo) : null;
        out.goLive = go ? (off(go) ? 'off' : 'on') : '';
        out.reason = reasonOf(dlg, go);
        var pub = dlg.querySelector('[role=radiogroup][aria-label=Visibility] [role=radio][aria-label=Public]');
        var priv = dlg.querySelector('[role=radiogroup][aria-label=Visibility] [role=radio][aria-label=Private]');
        out.visibility = pub && pub.getAttribute('aria-checked') === 'true' ? 'public' : priv && priv.getAttribute('aria-checked') === 'true' ? 'private' : '';
        out.options = all('[role=option]').filter(shown).map(function (o) { return words(o).slice(0, 60); });
      }
      var other = dialogs().filter(function (d) { return d !== dlg; })[0];
      out.other = other ? words(other).slice(0, 200) : '';
      // the Sources page
      out.sources = all('button[aria-label="Show stream key and encoder settings"]').length;
      out.createSourceButton = !!all('button[aria-label="Create Source"]').filter(shown)[0];
      return say(out);
    },

    openNew: function () {
      var b = buttonWithWords(/^new livestream$/i);
      if (!b || off(b)) return say({ ok: false });
      press(b);
      return say({ ok: true });
    },

    setTitle: function (text) {
      var dlg = createDialog();
      var input = dlg && (dlg.querySelector('#broadcast-title') || dlg.querySelector('input[placeholder^="Enter a title"]'));
      if (!input) return say({ ok: false });
      if (input.value !== text) setValue(input, text);
      return say({ ok: input.value === text });
    },

    tab: function (name) {
      var dlg = createDialog();
      var tab = dlg && all('button[role=tab]', dlg).filter(function (b) { return words(b).toLowerCase() === String(name).toLowerCase(); })[0];
      if (!tab) return say({ ok: false });
      if (tab.getAttribute('aria-selected') !== 'true') press(tab);
      return say({ ok: true });
    },

    // On the Audience step: who can watch. kind is "public" or "private".
    setVisibility: function (kind) {
      var dlg = createDialog();
      var group = dlg && dlg.querySelector('[role=radiogroup][aria-label=Visibility]');
      var radio = group && group.querySelector('[role=radio][aria-label=' + (kind === 'private' ? 'Private' : 'Public') + ']');
      if (!radio) return say({ ok: false });
      if (radio.getAttribute('aria-checked') !== 'true') press(radio);
      return say({ ok: true });
    },

    // Open the source list, or (with a name) pick the entry of that name, or (with "*green") the first one with a green dot.
    openSources: function () {
      var dlg = createDialog();
      var combo = dlg && dlg.querySelector('button[role=combobox]');
      if (!combo) return say({ ok: false });
      if (!all('[role=option]').some(shown)) press(combo);
      return say({ ok: true });
    },
    chooseSource: function (name) {
      var options = all('[role=option]').filter(shown);
      var wanted = String(name).toLowerCase();
      var pick = options.filter(function (o) { return words(o).toLowerCase() === wanted; })[0] || (wanted === '*green' ? options.filter(function (o) { return dotIsGreen(o) === true; })[0] : null);
      if (!pick) return say({ ok: false, count: options.length });
      press(pick);
      return say({ ok: true });
    },

    goLive: function () {
      var dlg = createDialog();
      var b = dlg && buttonWithWords(/^go live$/i, dlg);
      if (!b || off(b)) return say({ ok: false });
      press(b);
      return say({ ok: true });
    },

    closeDialog: function () {
      var dlg = createDialog() || dialogs()[0];
      var b = dlg && (dlg.querySelector('button[aria-label=Close]') || buttonWithWords(/^(close|cancel)$/i, dlg));
      if (b) { press(b); return say({ ok: true }); }
      var target = D.activeElement || D.body;
      ['keydown', 'keyup'].forEach(function (k) { target.dispatchEvent(new KeyboardEvent(k, { key: 'Escape', code: 'Escape', keyCode: 27, bubbles: true })); });
      return say({ ok: false });
    },

    // ---- the Sources page (studio.x.com/live/sources): find a source, show its key, read it
    // The rows of the list: the name is the first line of a row that is not the "Connected" / "Not connected" label, the region the next.
    rows: function () {
      return all('button[aria-label="Show stream key and encoder settings"]').map(function (button) {
        // the row is the nearest container that has the line with the region and the date in it ("US East (N. Virginia) \u00b7 Today"): with ONE source on the page there is no other
        // row to tell it apart by, and climbing to "the container that holds just this button" would end at the whole page
        var row = button.parentElement;
        while (row.parentElement && words(row).indexOf('\u00b7') < 0 && row.parentElement !== D.body) row = row.parentElement;
        var leaves = [], walker = D.createTreeWalker(row, NodeFilter.SHOW_TEXT), n;
        while ((n = walker.nextNode())) {
          var t = n.textContent.replace(/\s+/g, ' ').trim();
          if (t && !/^(connected|not connected)$/i.test(t) && t.indexOf('\u00b7') < 0) leaves.push(t);
        }
        var rowText = words(row);
        return { name: leaves[0] || '', region: leaves[1] || '', on: !/not connected/i.test(rowText) && /connected/i.test(rowText), button: button };
      });
    },
    openKey: function (name) {
      var rows = api.rows();
      if (!rows.length) return say({ ok: false, sources: 0 });
      var wanted = String(name || '').toLowerCase();
      var row = (wanted && rows.filter(function (r) { return r.name.toLowerCase() === wanted; })[0]) || rows[0];
      press(row.button);
      return say({ ok: true, sources: rows.length, name: row.name, region: row.region });
    },
    // The panel that opened ("Encoder setup"): reveals the key and reads the server address and the key. Returns keyMasked while the page still shows dots.
    readKey: function () {
      var dlg = dialogs().filter(function (d) { return /encoder setup/i.test(words(d).slice(0, 60)); })[0];
      if (!dlg) return say({ ok: false });
      var eye = dlg.querySelector('[aria-label="Show stream key"]');
      if (eye) { press(eye); return say({ ok: false, pending: true }); }
      var keyButton = dlg.querySelector('button[aria-label="Copy Stream key"]');
      var urlButton = dlg.querySelector('button[aria-label="Copy RTMP"]');
      var key = keyButton ? words(keyButton) : '', url = urlButton ? words(urlButton) : '';
      if (!key || /^[•*]+$/.test(key)) return say({ ok: false, pending: true });
      return say({ ok: true, key: key, url: url });
    },
    createSource: function (name) {
      var dlg = dialogs().filter(function (d) { return /create source/i.test(words(d).slice(0, 40)); })[0];
      if (!dlg) {
        var open = all('button[aria-label="Create Source"]').filter(shown)[0];
        if (!open) return say({ ok: false });
        press(open);
        return say({ ok: true, step: 'opened' });
      }
      var input = dlg.querySelector('input');
      if (input && input.value !== String(name)) setValue(input, String(name));   // X starts it on a name of its own ("Source 1"): the person's source is called what MOBS asks
      var go = buttonWithWords(/^create source$/i, dlg);
      if (!go || off(go)) return say({ ok: false, step: 'waiting' });
      press(go);
      return say({ ok: true, step: 'created' });
    },
    closePanel: function () {
      var target = D.activeElement || D.body;
      ['keydown', 'keyup'].forEach(function (k) { target.dispatchEvent(new KeyboardEvent(k, { key: 'Escape', code: 'Escape', keyCode: 27, bubbles: true })); });
      return say({ ok: true });
    }
  };
  window.__mobsPilot = api;
  return 'ready';
})()
