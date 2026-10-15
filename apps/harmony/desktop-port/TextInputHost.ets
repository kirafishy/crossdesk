import { inputMethod } from '@kit.IMEKit';
import { window } from '@kit.ArkUI';
import { DisplayHost } from './DisplayHost';
import type { DesktopEvent } from './DesktopHost';

// The OS owns composition/candidates; Slint owns the text, selection and edits.
export class TextInputHost {
  private static editor: inputMethod.InputMethodController = inputMethod.getController();
  private static id: number = 0;
  private static preview: string = '';
  private static queue: Promise<void> = Promise.resolve();
  private static revision: number = 0;
  private static requestedId: number = 0;
  private static requestedSession: number = 0;

  static request(event: DesktopEvent, target: window.Window, send: (event: DesktopEvent) => void): void {
    const id = event.id ?? 0;
    if (event.kind === 1) {
      TextInputHost.requestedId = id;
      TextInputHost.requestedSession = event.imeSession ?? 0;
      ++TextInputHost.revision;
    } else if (event.kind === 0) {
      if (TextInputHost.requestedId !== id) return;
      TextInputHost.requestedId = 0;
      ++TextInputHost.revision;
    } else if (TextInputHost.requestedId !== id || TextInputHost.requestedSession !== (event.imeSession ?? 0)) return;
    const revision = TextInputHost.revision;
    TextInputHost.queue = TextInputHost.queue.then(() => TextInputHost.apply(event, target, send, revision))
      .catch((error: Error) => console.error(`Slint input method: ${error.message}`));
  }

  static release(id: number): void {
    if (TextInputHost.requestedId === id) {
      TextInputHost.requestedId = 0;
      ++TextInputHost.revision;
    }
    TextInputHost.queue = TextInputHost.queue.then(async () => {
      if (TextInputHost.id !== id || TextInputHost.requestedId === id) return;
      TextInputHost.id = 0; TextInputHost.preview = '';
      await TextInputHost.editor.detach();
    }).catch((error: Error) => console.warn(`Slint input cleanup: ${error.message}`));
  }

  private static async apply(event: DesktopEvent, target: window.Window, send: (event: DesktopEvent) => void,
    revision: number): Promise<void> {
    if (revision !== TextInputHost.revision) return;
    const id = event.id ?? 0;
    const editor = TextInputHost.editor;
    if (event.kind === 0) {
      if (TextInputHost.id === id) { TextInputHost.id = 0; TextInputHost.preview = ''; await editor.detach(); }
      return;
    }
    const rect = target.getWindowProperties().windowRect;
    const scale = DisplayHost.screen(target).densityPixels;
    const cursor: inputMethod.CursorInfo = { left: rect.left + (event.x ?? 0) * scale,
      top: rect.top + ((event.y ?? 0) + (target.getWindowDecorVisible() ? target.getWindowDecorHeight() : 0)) * scale,
      width: Math.max(1, (event.width ?? 1) * scale), height: Math.max(1, (event.height ?? 1) * scale) };
    const selection: inputMethod.Range = { start: event.anchor ?? 0, end: event.cursor ?? 0 };
    const inputType = event.inputType === 1 ? inputMethod.TextInputType.VISIBLE_PASSWORD :
      event.inputType === 2 ? inputMethod.TextInputType.NUMBER :
      event.inputType === 3 ? inputMethod.TextInputType.NUMBER_DECIMAL : inputMethod.TextInputType.TEXT;
    if (TextInputHost.id !== id || event.kind === 1) {
      if (TextInputHost.id !== 0) { TextInputHost.id = 0; await editor.detach(); }
      if (revision !== TextInputHost.revision) return;
      // Bind the input session to the UI context of its owning window.
      await editor.attachWithUIContext(target.getUIContext(), { inputAttribute: { textInputType: inputType,
        enterKeyType: inputMethod.EnterKeyType.DONE, consumeKeyEvents: true }, cursorInfo: cursor, selection,
        windowId: target.getWindowProperties().id }, { showKeyboard: false });
      if (revision !== TextInputHost.revision) { await editor.detach(); return; }
      TextInputHost.id = id; TextInputHost.preview = '';
      const current = (): boolean => TextInputHost.id === id && revision === TextInputHost.revision;
      const deliver = (command: DesktopEvent): void => {
        if (!current()) return;
        command.imeSession = event.imeSession ?? 0;
        send(command);
      };
      const key = (code: number): void => {
        deliver({ op: 'key', id, code, down: true }); deliver({ op: 'key', id, code, down: false });
      };
      // off() is required when the same process attaches a new text field.
      editor.off('insertText'); editor.off('deleteLeft'); editor.off('deleteRight');
      editor.off('sendFunctionKey'); editor.off('setPreviewText'); editor.off('finishTextPreview');
      editor.off('handleExtendAction'); editor.off('moveCursor');
      editor.off('selectByRange'); editor.off('selectByMovement');
      editor.on('insertText', (text: string) => {
        if (!current()) return;
        TextInputHost.preview = ''; deliver({ op: 'composition', id, text, commit: true });
      });
      editor.on('setPreviewText', (text: string, _range: inputMethod.Range) => {
        if (!current()) return;
        TextInputHost.preview = text; deliver({ op: 'composition', id, text, commit: false });
      });
      editor.on('finishTextPreview', () => {
        if (!current()) return;
        if (TextInputHost.preview.length > 0) deliver({ op: 'composition', id, text: TextInputHost.preview, commit: true });
        TextInputHost.preview = '';
      });
      editor.on('deleteLeft', (length: number) => { for (let i = 0; i < Math.min(length, 65536); ++i) key(2055); });
      editor.on('deleteRight', (length: number) => { for (let i = 0; i < Math.min(length, 65536); ++i) key(2071); });
      editor.on('sendFunctionKey', () => key(2054));
      // The input method consumes these hardware shortcuts while attached.
      // Deliver editing commands to the focused Slint TextInput, without
      // synthesizing modifier presses that could disturb held physical keys.
      editor.on('handleExtendAction', (action: inputMethod.ExtendAction) => {
        let code = 0;
        if (action === inputMethod.ExtendAction.SELECT_ALL) code = 2017;
        else if (action === inputMethod.ExtendAction.CUT) code = 2040;
        else if (action === inputMethod.ExtendAction.COPY) code = 2019;
        else if (action === inputMethod.ExtendAction.PASTE) code = 2038;
        if (code !== 0) deliver({ op: 'imeKey', id, code, control: true });
      });
      const move = (direction: inputMethod.Direction, shift: boolean): void => {
        const code = direction === inputMethod.Direction.CURSOR_UP ? 2012 :
          direction === inputMethod.Direction.CURSOR_DOWN ? 2013 :
          direction === inputMethod.Direction.CURSOR_LEFT ? 2014 :
          direction === inputMethod.Direction.CURSOR_RIGHT ? 2015 : 0;
        if (code !== 0) deliver({ op: 'imeKey', id, code, shift });
      };
      editor.on('moveCursor', (direction: inputMethod.Direction) => move(direction, false));
      editor.on('selectByMovement', (movement: inputMethod.Movement) => move(movement.direction, true));
      editor.on('selectByRange', (range: inputMethod.Range) => {
        deliver({ op: 'imeSelection', id, anchor: range.start, cursor: range.end });
      });
    }
    await editor.changeSelection(event.text ?? '', selection.start, selection.end);
    if (revision !== TextInputHost.revision) return;
    await editor.updateCursor(cursor);
  }
}
