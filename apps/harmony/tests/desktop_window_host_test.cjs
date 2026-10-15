const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const ts = require(process.argv[2]);

// Exercise the production bridge with asynchronous native windows, including
// callbacks that outlive a destroyed XComponent/window with the same Slint id.
const states = { FLOATING: 1, MAXIMIZE: 2, FULL_SCREEN: 3, MINIMIZE: 4 };
const modes = { FOLLOW_APP_IMMERSIVE_SETTING: 0, EXIT_IMMERSIVE: 1, ENTER_IMMERSIVE: 2 };
const events = { WINDOW_ACTIVE: 1, WINDOW_INACTIVE: 2 };
const sent = [], released = [], moduleExports = {};
let commands = [], nextNativeId = 10;
class NativeWindow {
  constructor(status = states.FLOATING) {
    this.id = ++nextNativeId; this.status = status; this.previous = states.FLOATING;
    this.handlers = new Map(); this.calls = []; this.decor = true;
  }
  getWindowStatus() { return this.status; }
  getUIContext() { return {}; }
  getWindowProperties() { return { id: this.id, windowRect: { left: 20, top: 30 } }; }
  getWindowDecorVisible() { return this.decor; }
  getWindowDecorHeight() { return 32; }
  setWindowDecorVisible(value) { this.decor = value; }
  async setWindowTitle() { if (this.rejectTitle) throw Error('unsupported'); }
  async setSubWindowZLevel() { if (this.rejectStack) throw Error('unsupported'); }
  async setWindowLimits(limits) { this.limits = limits; this.calls.push('limits'); }
  async loadContent() { this.calls.push('load'); }
  async resize(w, h) { this.calls.push(['resize', w, h]); }
  async destroyWindow() { this.calls.push('destroy'); }
  async minimize() { this.calls.push('minimize'); this.previous = this.status; this.status = states.MINIMIZE; }
  async restore() { this.calls.push('restore'); this.status = this.previous; }
  async showWindow() {
    this.calls.push('show');
    // showWindow does not restore a minimized 2in1 main window.
    if (this !== main && this.status === states.MINIMIZE) this.status = this.previous;
  }
  async recover() { this.calls.push('recover'); this.status = states.FLOATING; }
  async maximize(mode) {
    this.calls.push(['maximize', mode]);
    this.status = mode === modes.EXIT_IMMERSIVE ? states.MAXIMIZE : states.FULL_SCREEN;
  }
  on(name, callback) { if (name === this.unsupportedEvent) throw Error('unsupported'); this.handlers.set(name, callback); }
  off(name) { this.handlers.delete(name); }
  async createSubWindowWithOptions() { return new NativeWindow(); }
}
const main = new NativeWindow();
vm.runInNewContext(ts.transpileModule(fs.readFileSync(process.argv[3], 'utf8'), {
  compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 }
}).outputText, {
  exports: moduleExports, console: { warn: () => {}, error: e => { throw Error(e); } },
  LocalStorage: class { setOrCreate() {} },
  require: name => {
    if (name === 'libslint_desktop.so') return { default: {
      send: text => sent.push(JSON.parse(text)), poll: () => JSON.stringify(commands.splice(0))
    } };
    if (name === '@kit.ArkUI') return { window: { WindowStatusType: states, MaximizePresentation: modes, WindowEventType: events } };
    if (name === './DesktopAccess') return { DesktopAccess: { refresh() {}, requestHost() {} } };
    if (name === './TextInputHost') return { TextInputHost: { release: id => released.push(id) } };
    if (name === './DisplayHost') return { DisplayHost: {
      screen: () => ({ densityPixels: 2 }), global: () => ({ x: 20, y: 30 }),
      move: async (target, x, y) => target.calls.push(['move', x, y])
    } };
    return {};
  }
});
const host = moduleExports.DesktopHost;
host.stage = { getMainWindowSync: () => main };
host.windows.set(1, main);
const props = (id, changes = {}) => ({ op: 'properties', id, stateRevision: host.stateRevisions.get(id) ?? 0,
  fullscreen: false, maximized: false, minimized: false, ...changes });

(async () => {
  main.status = states.FULL_SCREEN;
  await host.windowCommand(props(1, { maximized: true }));
  assert.equal(main.status, states.MAXIMIZE, 'leaving fullscreen must explicitly exit immersive mode');
  await host.windowCommand(props(1, { fullscreen: true, minimized: true }));
  assert.equal(main.status, states.MINIMIZE, 'minimized takes precedence over fullscreen');
  await host.windowCommand(props(1));
  assert.equal(main.status, states.FLOATING, 'unminimize must finish the requested mode transition');
  assert(main.calls.includes('restore')); assert(main.calls.includes('recover'));

  await host.windowCommand({ op: 'visible', id: 1, visible: true, width: 900, height: 600 });
  main.calls = []; main.status = states.MAXIMIZE;
  await host.windowCommand({ op: 'visible', id: 1, visible: true, width: 3000, height: 1800 });
  assert.deepEqual(main.calls, ['show'], 'showing a visible maximized main window must not reset its restore size');
  main.rejectTitle = true;
  await host.windowCommand(props(1, { title: 'new title', fullscreen: true }));
  assert.equal(main.status, states.FULL_SCREEN, 'optional chrome failure cannot abort fullscreen');

  await host.windowCommand(props(2, { topmost: true, noFrame: true, title: 'Shared panel' }));
  await host.windowCommand({ op: 'visible', id: 2, visible: true, width: 400, height: 250, x: -100, y: 40 });
  const old = host.windows.get(2);
  assert(old.calls.some(c => Array.isArray(c) && c[0] === 'move' && c[1] === -100));
  assert(!old.decor);
  const late = new Map(old.handlers);
  old.handlers.get('windowStatusDidChange')(states.MINIMIZE);
  assert.deepEqual(sent.at(-1), { op: 'state', id: 2, stateRevision: 1, minimized: true });
  await host.windowCommand({ op: 'visible', id: 2, visible: false });
  assert.equal(old.handlers.size, 0); assert(released.includes(2));
  await host.windowCommand({ op: 'visible', id: 2, visible: true, width: 400, height: 250 });
  assert.notEqual(host.windows.get(2), old);
  const before = sent.length;
  late.get('windowEvent')(events.WINDOW_INACTIVE);
  late.get('windowRectChange')(); late.get('displayIdChange')();
  late.get('windowStatusDidChange')(states.FULL_SCREEN);
  assert.equal(await late.get('windowWillClose')(), false);
  assert.equal(sent.length, before, 'destroyed native windows cannot update their replacement');

  const child = host.windows.get(2);
  child.rejectStack = true;
  await host.windowCommand(props(2, { topmost: true, maximized: true }));
  assert.equal(child.status, states.MAXIMIZE);
  child.status = states.FLOATING;
  child.handlers.get('windowStatusDidChange')(states.FLOATING);
  await host.windowCommand(props(2, { maximized: true, stateRevision: 0 }));
  assert.equal(child.status, states.FLOATING, 'an old mode command cannot undo a native titlebar restore');
  let finishLimits;
  child.setWindowLimits = async () => { await new Promise(resolve => { finishLimits = resolve; }); };
  const pendingMode = host.windowCommand(props(2, { maximized: true }));
  await new Promise(resolve => setImmediate(resolve));
  child.handlers.get('windowStatusDidChange')(states.FLOATING);
  finishLimits(); await pendingMode;
  assert.equal(child.status, states.FLOATING, 'a native transition during an awaited property operation also wins');
  child.setWindowLimits = async () => {};
  child.status = states.MAXIMIZE;
  await host.windowCommand({ op: 'properties', id: 2, title: 'layout update' });
  assert.equal(child.status, states.MAXIMIZE, 'an unrelated property change must not recover the native window');
  child.unsupportedEvent = 'displayIdChange';
  host.observe(2, child);
  assert(child.handlers.has('windowWillClose'), 'one unavailable observer must not disable close interception');
  commands = [{ op: 'destroy', id: 2 }, { op: 'destroy', id: 9 }];
  host.poll();
  await Promise.all([...host.queues.values()]);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(host.queues.size, 0);
  assert.equal(host.properties.has(2), false);
  assert.equal(host.windows.has(2), false);
  const broken = new NativeWindow();
  broken.loadContent = async () => { throw Error('creation failed'); };
  main.createSubWindowWithOptions = async () => broken;
  await assert.rejects(host.windowCommand({ op: 'visible', id: 3, visible: true }), /creation failed/);
  assert.equal(host.windows.has(3), false); assert(broken.calls.includes('destroy'));
  console.log('Slint window restore/fullscreen, optional services, recreation, stale callbacks and queue cleanup passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
