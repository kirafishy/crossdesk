const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const ts = require(process.argv[2]);

// Run the production ArkTS service with a fake IME. In particular, commands
// consumed by the OS must reach Slint exactly once after field reattachment.
const handlers = new Map();
let attached = 0, detached = 0, selection;
const editor = {
  attachWithUIContext: async (context, config, options) => {
    assert.equal(context, 17); assert.equal(options.showKeyboard, false);
    assert.equal(config.inputAttribute.consumeKeyEvents, true); ++attached;
  },
  detach: async () => { ++detached; },
  on: (type, callback) => { assert(!handlers.has(type)); handlers.set(type, callback); },
  off: type => handlers.delete(type),
  changeSelection: async (...value) => { selection = value; }, updateCursor: async () => {}
};
const inputMethod = {
  getController: () => editor,
  TextInputType: { TEXT: 0, VISIBLE_PASSWORD: 1, NUMBER: 2, NUMBER_DECIMAL: 3 },
  EnterKeyType: { DONE: 1 },
  Direction: { CURSOR_UP: 1, CURSOR_DOWN: 2, CURSOR_LEFT: 3, CURSOR_RIGHT: 4 },
  ExtendAction: { SELECT_ALL: 0, CUT: 3, COPY: 4, PASTE: 5 }
};
const source = fs.readFileSync(process.argv[3], 'utf8');
const compiled = ts.transpileModule(source, { compilerOptions: {
  module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020
}}).outputText;
const exportsObject = {};
vm.runInNewContext(compiled, { exports: exportsObject, console, require: name => {
  if (name === '@kit.IMEKit') return { inputMethod };
  if (name === './DisplayHost') return { DisplayHost: { screen: () => ({ densityPixels: 2 }) } };
  throw Error(`Unexpected import ${name}`);
}});
const host = exportsObject.TextInputHost;
const target = { getWindowProperties: () => ({ id: 17, windowRect: { left: 20, top: 30 } }),
  getWindowDecorVisible: () => false, getWindowDecorHeight: () => 0, getUIContext: () => 17 };
const sent = [];
const request = async event => {
  host.request(event, target, value => sent.push(value));
  await new Promise(resolve => setImmediate(resolve));
};
const take = () => JSON.parse(JSON.stringify(sent.splice(0)));
const keyboardModule = {};
vm.runInNewContext(ts.transpileModule(fs.readFileSync(process.argv[4], 'utf8'), {
  compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 }
}).outputText, { exports: keyboardModule, KeyType: { Down: 0, Up: 1 } });
const keyboard = new keyboardModule.KeyboardHost();
const key = (code, down, ctrl = false) => keyboard.preIme({ keyCode: code,
  type: down ? 0 : 1, getModifierKeyState: keys => {
    assert(['Ctrl', 'Alt', 'Shift'].includes(keys[0]));
    return keys[0] === 'Ctrl' && ctrl;
  } }, 1, e => sent.push(e));
assert(key(2072, true, true)); // Slint gets the modifier even if the IME consumes it.
assert(key(2017, true, true));
assert(key(2017, true, true)); // Repeat is retained.
assert(key(2072, false));
assert(key(2017, false)); // Ctrl was released first.
assert.equal(take().length, 5);
assert(!key(2017, true)); // Plain typing remains with the IME.
assert(!key(2050, true, true)); // Ctrl+Space can switch input methods.
assert(key(2019, true, true)); take(); keyboard.reset();
assert(!key(2019, true)); // Lost focus must not consume the next plain letter.
assert(key(2076, true)); assert(key(2017, true));
assert(key(2076, false)); assert(key(2017, false));
assert.equal(take().length, 4);

async function verifyClipboardQueue() {
  const module = {}, replies = [], order = [];
  let clipboardPermission = true;
  let pending = [], releaseRead, clipboardText = 'old';
  const native = { poll: () => JSON.stringify(pending.splice(0)),
    send: value => replies.push(JSON.parse(value)) };
  const read = new Promise(resolve => { releaseRead = resolve; });
  let readWait = read;
  const board = {
    hasDataType: () => true,
    getData: async () => { order.push('read'); const text = clipboardText; await readWait; return { getPrimaryText: () => text }; },
    setData: async data => { order.push(data); if (data === 'fail') throw Error('denied'); }
  };
  vm.runInNewContext(ts.transpileModule(fs.readFileSync(process.argv[5], 'utf8'), {
    compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 }
  }).outputText, { exports: module, console, require: name => {
    if (name === 'libslint_desktop.so') return { default: native };
    if (name === './DesktopAccess') return { DesktopAccess: { requestClipboard: () => {} } };
    if (name === '@kit.BasicServicesKit') return { pasteboard: {
      getSystemPasteboard: () => board, MIMETYPE_TEXT_PLAIN: 'text/plain', createData: (_, text) => text
    } };
    if (name === '@kit.AbilityKit') return { abilityAccessCtrl: {
      GrantStatus: { PERMISSION_GRANTED: 0 }, createAtManager: () => ({
        checkAccessTokenSync: () => clipboardPermission ? 0 : -1
      })
    } };
    return {}; // Other OS services are not entered by this test.
  }});
  const host = module.DesktopHost;
  host.context = { applicationInfo: { accessTokenId: 17 } };
  pending = [{ op: 'clipboardRead', request: 1 }, { op: 'clipboardWrite', request: 2, text: 'new' }];
  host.poll();
  await new Promise(resolve => setImmediate(resolve));
  assert.deepEqual(order, ['read']); // Write must wait for the outstanding read.
  pending = [{ op: 'unknown', request: 3 }]; host.poll();
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(replies[0].request, 3); // Unrelated services remain responsive.
  releaseRead(); await host.clipboardQueue;
  assert.deepEqual(order, ['read', 'new']);
  assert.deepEqual(replies.slice(1).map(r => r.request), [1, 2]);
  pending = [{ op: 'clipboardWrite', request: 4, text: 'fail' }, { op: 'clipboardWrite', request: 5, text: 'recovered' }];
  host.poll(); await host.clipboardQueue;
  assert.equal(replies[3].ok, false); assert.equal(replies[4].ok, true);
  clipboardPermission = false;
  const before = order.length;
  pending = [{ op: 'clipboardRead', request: 6 }, { op: 'clipboardHas', request: 7 }];
  host.poll(); await host.clipboardQueue;
  assert.equal(order.length, before); // Polling cannot consume Ctrl+V's one-read grant.
  assert.equal(replies[5].ok, false); assert.equal(replies[6].ok, false);
  pending = [{ op: 'clipboardPaste', request: 8 }];
  host.poll(); await host.clipboardQueue;
  assert.equal(order.length, before + 1); assert.equal(replies[7].text, 'old');
  // No active window is required: an OS update invalidates the cache and
  // enters the same SDL notification path while the application is hidden.
  host.started = true; host.clipboardObserved = true; clipboardPermission = true;
  const query = async id => {
    pending = [{ op: 'clipboardRead', request: id }]; host.poll(); await host.clipboardQueue;
    return replies.find(r => r.request === id);
  };
  host.clipboardChanged();
  assert.equal((await query(9)).text, 'old');
  const afterFirst = order.length;
  assert.equal((await query(10)).text, 'old'); assert.equal(order.length, afterFirst);
  clipboardText = '外部应用复制 😀'; host.clipboardChanged();
  assert.equal((await query(11)).text, clipboardText); assert.equal(order.length, afterFirst + 1);
  // A change during an asynchronous read must not be cached as the new value.
  let releaseRace;
  readWait = new Promise(resolve => { releaseRace = resolve; });
  clipboardText = 'first'; host.clipboardChanged();
  const racingRead = query(12);
  await new Promise(resolve => setImmediate(resolve));
  clipboardText = 'second'; host.clipboardChanged(); releaseRace();
  assert.equal((await racingRead).text, 'first');
  assert.equal((await query(13)).text, 'second');
  // Revocation during getData must not expose or cache the result.
  readWait = new Promise(resolve => { releaseRace = resolve; });
  host.clipboardChanged(); const revokedRead = query(14);
  await new Promise(resolve => setImmediate(resolve));
  clipboardPermission = false; host.clipboardChanged(); releaseRace();
  assert.equal((await revokedRead).ok, false);
  const afterRevocation = order.length;
  assert.equal((await query(15)).ok, false); assert.equal(order.length, afterRevocation);
  assert.equal(host.clipboardCache, '');
  host.terminating = true;
  const notifications = replies.length; host.clipboardChanged();
  assert.equal(replies.length, notifications); // Late callbacks after shutdown are ignored.
}

async function verifyDesktopAccess() {
  const module = {}, notifications = [], calls = [], grants = new Set();
  let change, releasePrompt, watched;
  const manager = {
    checkAccessTokenSync: (_, permission) => grants.has(permission) ? 0 : -1,
    on: (type, permissions, callback) => { assert.equal(type, 'selfPermissionStateChange'); watched = Array.from(permissions); change = callback; },
    off: (type, permissions, callback) => { assert.equal(callback, change); change = undefined; },
    requestPermissionsFromUser: async () => { calls.push('clipboard'); await new Promise(resolve => { releasePrompt = resolve; }); },
    openPermissionOnSetting: async () => { calls.push('keyboard'); throw Error('user dismissed'); }
  };
  vm.runInNewContext(ts.transpileModule(fs.readFileSync(process.argv[6], 'utf8'), {
    compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 }
  }).outputText, { exports: module, console: { warn: () => {} }, require: name => {
    assert.equal(name, '@kit.AbilityKit'); return { abilityAccessCtrl: {
      GrantStatus: { PERMISSION_GRANTED: 0 }, createAtManager: () => manager
    } };
  }});
  let selection = 1, blockGuide = false, failSettings = false;
  const dialogs = [], routes = [];
  const ui = { getPromptAction: () => ({ showDialog: async options => {
    calls.push('guide'); dialogs.push(options);
    if (blockGuide) return await new Promise(resolve => { releasePrompt = resolve; });
    return { index: selection };
  } }) };
  const access = module.DesktopAccess, context = {
    applicationInfo: { accessTokenId: 17, name: 'com.chunkdash.crossdesk' },
    startAbility: async want => {
      calls.push('settings'); routes.push(JSON.parse(JSON.stringify(want)));
      if (failSettings) throw Error('settings unavailable');
    }
  };
  access.start(context, false, e => notifications.push(e));
  access.requestKeyboard(); access.requestClipboard(); access.requestHost(); await access.requests;
  assert.equal(calls.length, 0); assert.equal(change, undefined); // Ordinary HAP never opens unusable prompts.
  access.stop(); access.start(context, true, e => notifications.push(e));
  access.requestClipboard(); access.requestClipboard(); access.requestKeyboard();
  await new Promise(resolve => setImmediate(resolve));
  assert.deepEqual(calls, ['clipboard']); // Permission UIs must not overlap.
  releasePrompt(); await access.requests;
  assert.deepEqual(calls, ['clipboard', 'keyboard']);
  access.requestKeyboard(); access.requestClipboard(); await access.requests;
  assert.equal(calls.length, 2); // Denial/dismissal is not a retry loop.
  grants.add('ohos.permission.HOOK_KEY_EVENT'); change({});
  assert.equal(notifications.filter(e => e.op === 'keyboardPermission').at(-1).ok, true);
  grants.clear(); change({});
  assert.equal(notifications.filter(e => e.op === 'keyboardPermission').at(-1).ok, false);
  const late = change; access.stop();
  const before = notifications.length; late({});
  assert.equal(notifications.length, before);

  calls.length = 0;
  const screen = 'ohos.permission.CUSTOM_SCREEN_RECORDING', control = 'ohos.permission.CONTROL_DEVICE';
  blockGuide = true;
  access.start(context, false, e => notifications.push(e), true);
  assert.deepEqual(watched, [screen, control]); // Host-only builds do not require controller ACLs.
  access.requestHost(ui); access.requestHost(ui); access.requestKeyboard(); access.requestClipboard();
  await new Promise(resolve => setImmediate(resolve));
  assert.deepEqual(calls, ['guide']);
  assert.match(dialogs.at(-1).message, /屏幕共享/); assert.match(dialogs.at(-1).message, /辅助操作/);
  releasePrompt({ index: 1 }); await access.requests;
  assert.deepEqual(calls, ['guide', 'settings']);
  assert.deepEqual(routes[0], {
    bundleName: 'com.huawei.hmos.settings', abilityName: 'com.huawei.hmos.settings.MainAbility',
    uri: 'application_info_entry', parameters: { pushParams: 'com.chunkdash.crossdesk' }
  });
  const latest = () => notifications.filter(e => e.op === 'hostPermissions').at(-1);
  // Navigating to Settings is not a grant. Only system state changes enable access.
  assert.equal(latest().controlDevice, false); assert.equal(latest().screenRecording, false);
  grants.add(screen); grants.add(control); change({});
  assert.equal(latest().controlDevice, true); assert.equal(latest().screenRecording, true);
  access.stop(); access.start(context, false, e => notifications.push(e), true);
  access.requestHost(ui); await access.requests;
  assert.deepEqual(calls, ['guide', 'settings']); // Existing system grants survive an app restart.
  grants.delete(control); change({});
  assert.equal(latest().controlDevice, false); assert.equal(latest().screenRecording, true);
  access.stop();

  // Only explain the missing switch. Declining never launches Settings or loops.
  calls.length = 0; blockGuide = false; selection = 0;
  access.start(context, false, e => notifications.push(e), true);
  access.requestHost(ui); await access.requests;
  access.requestHost(ui); await access.requests;
  assert.deepEqual(calls, ['guide']);
  assert.match(dialogs.at(-1).message, /辅助操作/); assert.doesNotMatch(dialogs.at(-1).message, /屏幕共享/);
  assert.equal(latest().controlDevice, false); assert.equal(latest().screenRecording, true);
  grants.delete(screen); change({}); assert.equal(latest().screenRecording, false);
  access.stop();

  // A response from an old guide cannot navigate or publish state after restart.
  calls.length = 0; blockGuide = true; selection = 1;
  access.start(context, false, e => notifications.push(e), true);
  access.requestHost(ui); await new Promise(resolve => setImmediate(resolve));
  access.stop(); access.start(context, false, e => notifications.push(e), false);
  const beforeCompletion = notifications.length;
  releasePrompt({ index: 1 }); await access.requests;
  assert.deepEqual(calls, ['guide']); assert.equal(notifications.length, beforeCompletion);
  access.stop();

  // A failed Settings route gives manual guidance without granting access or
  // falling back to the system dialog with the misleading navigation path.
  calls.length = 0; blockGuide = false; failSettings = true;
  access.start(context, false, e => notifications.push(e), true);
  access.requestHost(ui); await access.requests;
  assert.deepEqual(calls, ['guide', 'settings', 'guide']);
  assert.equal(dialogs.at(-1).title, '未能打开应用设置');
  assert.match(dialogs.at(-1).message, /屏幕共享/); assert.match(dialogs.at(-1).message, /辅助操作/);
  assert.equal(latest().controlDevice, false); assert.equal(latest().screenRecording, false);
  access.stop();
}

(async () => {
  await verifyClipboardQueue();
  await verifyDesktopAccess();
  await request({ op: 'ime', id: 1, kind: 1, text: 'a😀中', anchor: 1, cursor: 4 });
  assert.deepEqual(selection, ['a😀中', 1, 4]);
  for (const [action, code] of [[0, 2017], [3, 2040], [4, 2019], [5, 2038]]) {
    handlers.get('handleExtendAction')(action);
    assert.deepEqual(take(), [{ op: 'imeKey', id: 1, imeSession: 0, code, control: true }]);
  }
  handlers.get('moveCursor')(3);
  handlers.get('selectByMovement')({ direction: 4 });
  handlers.get('selectByRange')({ start: 1, end: 3 });
  assert.deepEqual(take(), [
    { op: 'imeKey', id: 1, imeSession: 0, code: 2014, shift: false },
    { op: 'imeKey', id: 1, imeSession: 0, code: 2015, shift: true },
    { op: 'imeSelection', id: 1, imeSession: 0, anchor: 1, cursor: 3 }
  ]);
  handlers.get('insertText')('你好');
  assert.deepEqual(take(), [{ op: 'composition', id: 1, imeSession: 0, text: '你好', commit: true }]);
  const oldInsert = handlers.get('insertText'), oldPreview = handlers.get('setPreviewText');
  const oldFinish = handlers.get('finishTextPreview');
  await request({ op: 'ime', id: 1, kind: 1 });
  oldInsert('must not enter the new field'); oldPreview('stale'); oldFinish();
  assert.equal(take().length, 0);
  assert.equal(host.preview, '');
  assert.equal(attached, 2); assert.equal(detached, 1);
  handlers.get('handleExtendAction')(0);
  assert.equal(take().length, 1);
  const lateInsert = handlers.get('insertText');
  await request({ op: 'ime', id: 1, kind: 0 });
  lateInsert('late'); handlers.get('deleteLeft')(1);
  assert.equal(take().length, 0);
  // A native attach may finish after the window is destroyed. No editor or
  // callbacks may be installed after release, even with the same window id.
  let finishAttach;
  editor.attachWithUIContext = async () => { await new Promise(resolve => { finishAttach = resolve; }); };
  host.request({ op: 'ime', id: 2, kind: 1, imeSession: 7 }, target, e => sent.push(e));
  await new Promise(resolve => setImmediate(resolve));
  host.release(2); finishAttach(); await host.queue;
  assert.equal(host.id, 0);
  editor.attachWithUIContext = async () => {};
  await request({ op: 'ime', id: 2, kind: 1, imeSession: 9 });
  handlers.get('insertText')('current');
  assert.deepEqual(take(), [{ op: 'composition', id: 2, imeSession: 9, text: 'current', commit: true }]);
  const currentSelection = selection;
  await request({ op: 'ime', id: 2, kind: 2, imeSession: 7, text: 'old update' });
  assert.equal(selection, currentSelection);
  host.release(1); await host.queue; // Destroying another window keeps this editor.
  assert.equal(host.id, 2);
  host.release(2); await host.queue;
  assert.equal(host.id, 0);
  console.log('Slint IME editing, selection, composition, detached callbacks and clipboard serialization passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
