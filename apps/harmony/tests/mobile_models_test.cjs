const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require(process.argv[2]);
const root = process.argv[3];
let timers = new Map(), nextTimer = 0;
function load(name) {
  const exports = {};
  vm.runInNewContext(ts.transpileModule(fs.readFileSync(path.join(root, name + '.ets'), 'utf8'), {
    compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 }
  }).outputText, { exports, console, setTimeout: fn => { timers.set(++nextTimer, fn); return nextTimer; },
    clearTimeout: id => timers.delete(id), Date, Map, Set, Number, Math, JSON,
    require: name => { if (name.startsWith('./')) return load(name.slice(2)); throw Error(name); } });
  return exports;
}
const { RemoteViewport, MouseInput, TouchInput, VideoSettings, NetworkStatistics, RecentPresence, asciiKey } = load('ControllerModels');
const viewport = new RemoteViewport(); viewport.fit(320, 200, 1920, 1080);
assert.equal(viewport.baseHeight, 180); assert.equal(viewport.top(), 10);
const before = viewport.x(100); viewport.zoom(2, 100, 100); assert.equal(viewport.x(100), before);
viewport.zoom(100, 160, 100); assert.equal(viewport.scale, 10);
viewport.pan(1e6, -1e6); assert.equal(viewport.offsetX, viewport.baseWidth * 4.5);
viewport.zoom(.00001, 160, 100); assert.equal(viewport.scale, 1); assert.equal(viewport.offsetX, 0);
assert(!viewport.contains(100, 0)); assert.equal(viewport.x(-100), 0);
const packets = [];
const mouse = new MouseInput((x, y, flag, wheel) => packets.push({ x, y, flag, wheel }));
const touch = new TouchInput(viewport, mouse);
function input(kind, points = []) { if (kind === 'down') touch.begin(); touch.touch(points.map(([x,y]) => ({x,y})), kind); }
input('down', [[100,100]]); input('move', [[120,100]]); input('up');
assert(packets.every(p => p.flag === 0)); // Relative movement never drags.
packets.length = 0; input('down', [[100,100]]); input('up'); assert.deepEqual(packets.map(p => p.flag), [1,2]);
packets.length = 0; input('down', [[100,100]]); touch.hold(); input('up'); assert.deepEqual(packets.map(p => p.flag), [3,4]);
touch.relative = false; packets.length = 0;
input('down', [[100,100]]); input('move', [[120,100]]); input('move', [[400,300]]); input('up');
assert.deepEqual(packets.map(p => p.flag), [0,1,0,0,2]); assert.equal(packets.at(-2).x, 1);
packets.length = 0;
input('down', [[100,100]]); input('down', [[100,100],[200,100]]); input('move', [[80,100],[220,100]]);
input('up', [[80,100]]); input('move', [[90,110]]); input('up');
assert(viewport.scale > 1); assert(packets.every(p => p.flag === 0));
viewport.reset(); packets.length = 0;
input('down', [[100,100]]); input('down', [[100,100],[200,100]]); input('move', [[100,125],[200,125]]); input('up');
assert.equal(packets.filter(p => p.flag === 7).length, 1);
mouse.button(true, true); const revision = mouse.revision;
packets.length = 0; input('down', [[100,100]]); input('move', [[110,100]]); input('up');
assert(mouse.left); assert(packets.every(p => p.flag === 0)); assert.equal(mouse.revision, revision);
mouse.button(false, true); mouse.drag(32,0,320,180); mouse.release();
assert.equal(packets.at(-2).flag, 2); assert.equal(packets.at(-1).flag, 4);
assert.equal(asciiKey('a'), 65); assert.equal(asciiKey('A'), 321); assert.equal(asciiKey('!'), 305); assert.equal(asciiKey('中'), 0);
const settings = new VideoSettings(1, 1280); assert.equal(settings.selection.quality, 0);
assert.equal(settings.begin({quality:1,frameRate:30,preference:1}), -1); settings.support(true);
const one = {quality:2,frameRate:60,preference:0}, two = {quality:1,frameRate:30,preference:2};
const id1 = settings.begin(one), id2 = settings.begin(two);
settings.receive(one, id1, true); assert(settings.pending); assert.equal(settings.applied.quality, 2);
settings.receive(two, id2, false); assert(!settings.pending); assert.equal(settings.selection.quality, 2);
const id3 = settings.begin(two); settings.expire(id2); assert(settings.pending); settings.expire(id3); assert.equal(settings.selection.quality, 2);
settings.receive(two,id3+1,true); assert.equal(settings.selection.quality,2);
settings.requestId = 0xffffffff; settings.appliedRequestId = 0xffffffff;
const wrapped = settings.begin(two); assert.equal(wrapped,0); settings.receive(two,wrapped,true); assert.equal(settings.selection.preference,2);
settings.support(false); assert(!settings.pending);
const network = new NetworkStatistics();
const traffic = {inbound:8000000,outbound:10000,loss:.02};
network.receive({rtt:40,mode:0,srtp:true,video:traffic,audio:traffic,data:traffic,total:traffic}, 100);
network.receive({rtt:80,mode:1,srtp:false,video:traffic,audio:traffic,data:traffic,total:traffic}, 200);
assert.equal(network.rtt,50); assert.equal(network.mode(201),'TURN 中继'); assert.equal(network.mode(3200),'—');
network.frame(6,7000,1920,1080,200); network.frame(6,1,1920,1080,250); assert.equal(network.fps(250),6); assert.equal(network.latency(250),7000);
assert.equal(network.fps(1200),0); network.reset(); assert.equal(network.width,0);
const subscriptions = []; const presence = new RecentPresence(ids => subscriptions.push([...ids]));
presence.watch(['111111111'],0); presence.connect(true,10); assert.equal(subscriptions.length,1);
presence.receive({type:'presence_update',id:'111111111',online:true},20); assert(!presence.online('111111111',21));
presence.receive({type:'presence',devices:[{id:'111111111',online:true},{id:'222222222',online:true}]},30);
assert(presence.online('111111111',31)); assert(!presence.online('222222222',31)); assert(!presence.online('111111111',60030));
presence.tick(30010); assert.equal(subscriptions.length,2); presence.connect(false,40000); presence.connect(true,41000); assert(!presence.online('111111111',41001));
const { AnnouncementInbox } = load('AnnouncementInbox');
const { announcementSpans, announcementByteLength, isAnnouncementURL } = load('AnnouncementText');
const spans = announcementSpans('说明\r\n[下载](HTTP://example.com/a(b))；https://example.com/end. <b>普通文本</b>');
assert.equal(spans.map(p=>p.text).join(''),'说明\n下载；https://example.com/end. <b>普通文本</b>');
assert.deepEqual(Array.from(spans.filter(p=>p.url),p=>p.url),['HTTP://example.com/a(b)','https://example.com/end']);
assert(!isAnnouncementURL('javascript:alert(1)'));assert(!isAnnouncementURL('https://a\\b'));assert(!isAnnouncementURL('https://'));
assert.equal(announcementByteLength('中'.repeat(80)),240);assert.equal(announcementByteLength('😀'),4);
const requests = []; const saved = new Map(); let failSave = false;
const inbox = new AnnouncementInbox(r => requests.push({...r}), s => saved.get(s) ?? '{}', (s,v) => { if(failSave)throw Error(); saved.set(s,v); });
inbox.configure('server:9099:identity'); inbox.connect(true);
function answer(request, entries, total, revision=1) { inbox.receive({type:'announcements',request_id:request.request_id,summary_only:request.summary_only,offset:request.offset,total,catalog_revision:revision,items:entries}); }
const entries = Array.from({length:21},(_,i)=>({id:i+1,revision:1,title:`Title ${i+1}`,body:'Body',updated_at:1700000000}));
const first = requests.at(-1); answer(first,entries,21); assert.equal(requests.at(-1).summary_only,false);
answer(requests.at(-1),entries.slice(0,20),21); assert.equal(inbox.items.length,20); assert.equal(inbox.unread,21);
inbox.markRead(inbox.items[0]); assert.equal(inbox.unread,20);
failSave = true; inbox.dismiss(inbox.items[0]); assert.equal(inbox.total,21); assert(inbox.saveFailed);
failSave = false; inbox.dismiss(inbox.items[0]); answer(requests.at(-1),entries.slice(20),21); assert.equal(inbox.items.length,20); assert.equal(inbox.total,20);
const stale = requests.at(-1); inbox.refresh(); answer(stale,[],0); assert(inbox.loading); answer(requests.at(-1),entries.map(e=>({...e,revision:2})),21,2);
answer(requests.at(-1),entries.slice(0,20).map(e=>({...e,revision:2})),21,2); assert.equal(inbox.unread,21);
inbox.more(); answer(requests.at(-1),entries.slice(20).map(e=>({...e,revision:2})),21,2); assert.equal(inbox.items.length,21);
inbox.connect(false); const count=requests.length; inbox.more(); assert.equal(requests.length,count);
inbox.connect(true); for(const timer of [...timers.values()])timer(); assert(inbox.failed);
inbox.configure('utf8');inbox.connect(true);answer(requests.at(-1),[entries[0]],1);
answer(requests.at(-1),[{...entries[0],title:'中'.repeat(81)}],1);assert(inbox.failed);assert.equal(inbox.items.length,0);
console.log('PASS: mobile viewport/gestures, shared button ownership, keyboard, video ACK ordering, network expiry, presence and announcement pagination/revisions');
