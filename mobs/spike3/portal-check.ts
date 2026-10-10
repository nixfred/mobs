// Throwaway portal check (bun, no installs): create a PipeWire screen-capture input in a hidden OBS,
// wait for the portal picker to be answered, record ~5 s, and report whether the frames are real.
import { createHash } from "node:crypto";
import { writeFileSync } from "node:fs";

const pass = process.env.MOBS_OBS_PASSWORD ?? "";
const dir = process.env.MOBS_REC_DIR ?? "/tmp";
const out = process.env.MOBS_SHOT ?? "/tmp/mobs-portal-shot.png";
const b64sha = (s: string | Buffer) => createHash("sha256").update(s).digest("base64");
const log = (s: string) => console.log(s);

const ws = new WebSocket("ws://127.0.0.1:4455");
const waiting = new Map<string, (d: any) => void>();
let hello: any, stopped: any;
let onHello: (d: any) => void, onIdentified: (d: any) => void;
const gotHello = new Promise<any>((r) => (onHello = r));
const gotIdent = new Promise<any>((r) => (onIdentified = r));

ws.onmessage = (ev) => {
  const m = JSON.parse(String(ev.data));
  if (m.op === 0) onHello(m.d);
  else if (m.op === 2) onIdentified(m.d);
  else if (m.op === 7) waiting.get(m.d.requestId)?.(m.d);
  else if (m.op === 5 && m.d.eventType === "RecordStateChanged" && m.d.eventData.outputState === "OBS_WEBSOCKET_OUTPUT_STOPPED") stopped = m.d.eventData;
};
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
let n = 0;
const req = (requestType: string, requestData?: any, timeout = 10000) =>
  new Promise<any>((res) => {
    const requestId = `r${++n}`;
    const t = setTimeout(() => res({ requestStatus: { result: false, comment: "timeout" } }), timeout);
    waiting.set(requestId, (d) => { clearTimeout(t); res(d); });
    ws.send(JSON.stringify({ op: 6, d: { requestType, requestId, requestData } }));
  });

hello = await Promise.race([gotHello, sleep(10000).then(() => null)]);
if (!hello) { log("FAIL no Hello"); process.exit(2); }
const id: any = { rpcVersion: 1, eventSubscriptions: 64 };
if (hello.authentication) {
  const secret = b64sha(pass + hello.authentication.salt);
  id.authentication = b64sha(secret + hello.authentication.challenge);
}
ws.send(JSON.stringify({ op: 1, d: id }));
await gotIdent;
log("PASS  authenticated");

await req("CreateScene", { sceneName: "MOBS" });
await req("SetCurrentProgramScene", { sceneName: "MOBS" });
const created = await req("CreateInput", { sceneName: "MOBS", inputName: "my screen", inputKind: "pipewire-screen-capture-source", inputSettings: { ShowCursor: true } });
log(`${created.requestStatus.result ? "PASS " : "FAIL "} screen input created`);
log("PICKER: a screen-share dialog should be open now. Choose your screen. Waiting up to 120 s...");

// The picker is answered when the source starts producing real frames: poll small screenshots.
let sized = 0, approvedAfter = -1;
for (let s = 0; s < 120; s++) {
  const r = await req("GetSourceScreenshot", { sourceName: "my screen", imageFormat: "png", imageWidth: 320, imageCompressionQuality: -1 }, 5000);
  const data: string = r.responseData?.imageData ?? "";
  sized = data.length;
  if (sized > 9000) { approvedAfter = s; writeFileSync(out, Buffer.from(data.split(",")[1] ?? "", "base64")); break; }
  await sleep(1000);
}
if (approvedAfter < 0) { log(`FAIL  no real frames after 120 s (last screenshot payload ${sized} chars)`); process.exit(1); }
log(`PASS  portal answered, real frames after ~${approvedAfter} s (screenshot payload ${sized} chars)`);

await req("SetRecordDirectory", { recordDirectory: dir });
const st = await req("StartRecord");
log(`${st.requestStatus.result ? "PASS " : "FAIL "} StartRecord`);
await sleep(5000);
const sp = await req("StopRecord", undefined, 20000);
const path = sp.responseData?.outputPath ?? "";
for (let i = 0; i < 300 && !stopped; i++) await sleep(50);
log(`${stopped ? "PASS " : "FAIL "} STOPPED event; path ${path}`);
log("OUTPUT " + path);
ws.close();
process.exit(stopped ? 0 : 1);
