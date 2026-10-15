import type { DesktopEvent } from './DesktopHost';

export class KeyboardHost {
  private captured: Set<number> = new Set();
  private modifiers: Set<number> = new Set();
  reset(): void { this.captured.clear(); this.modifiers.clear(); }

  static event(event: KeyEvent, id: number): DesktopEvent {
    return { op: 'key', id, code: event.keyCode, down: event.type === KeyType.Down,
      unicode: event.unicode, capsLock: event.isCapsLockOn, numLock: event.isNumLockOn };
  }

  preIme(event: KeyEvent, id: number, send: (event: DesktopEvent) => void): boolean {
    const code = event.keyCode;
    const modifier = code === 2072 || code === 2073 || code === 2045 || code === 2046 ||
      code === 2047 || code === 2048 || code === 2076 || code === 2077;
    if (modifier) {
      if (event.type === KeyType.Down) this.modifiers.add(code);
      else this.modifiers.delete(code);
    }
    // ArkUI's query accepts only Ctrl/Alt/Shift, not Meta. Track Meta through
    // its actual key transitions instead of issuing an invalid SDK query.
    const control = (event.getModifierKeyState?.(['Ctrl']) ?? false) ||
      this.modifiers.has(2072) || this.modifiers.has(2073);
    const shortcutModifier = code === 2072 || code === 2073 || code === 2076 || code === 2077;
    const shortcut = shortcutModifier || (!modifier && code !== 2050 &&
      (control || event.metaKey === 1 || this.modifiers.has(2076) || this.modifiers.has(2077)));
    // IMEs may turn Ctrl+A into insertText("a") instead of an edit command.
    // Slint must receive shortcuts as keys before the IME consumes them. Keep
    // the matching release even when the modifier is released first. Space
    // remains available for the input method's own toggle. Its native event
    // still carries the OS modifier state. Forward Ctrl/Meta transitions too:
    // some IMEs otherwise consume the down event but pass the key-up along.
    if (!shortcut && !this.captured.has(code)) return false;
    if (event.type === KeyType.Down) this.captured.add(code);
    else this.captured.delete(code);
    send(KeyboardHost.event(event, id));
    return true;
  }
}
