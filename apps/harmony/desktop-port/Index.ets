import { DesktopEvent, DesktopHost } from './DesktopHost';
import { KeyboardHost } from './KeyboardHost';


// This host owns only the native surface and event delivery. All visible
// application controls are compiled from apps/desktop's existing Slint files.
@Entry({ useSharedStorage: true })
@Component
struct Index {
  private surface: XComponentController = new XComponentController();
  private keyboard: KeyboardHost = new KeyboardHost();
  private surfaceId: string = '';

  private send(event: DesktopEvent): void {
    event.surface = this.surfaceId;
    DesktopHost.send(event);
  }
  @LocalStorageProp('slintWindowId') private windowId: number = 1;

  aboutToDisappear(): void {
    this.keyboard.reset();
    if (this.surfaceId.length === 0) return;
    this.send({ op: 'detach', id: this.windowId });
    this.surfaceId = '';
  }

  build() {
    Stack() {
      XComponent({ id: 'slint-desktop', type: XComponentType.SURFACE, controller: this.surface })
        .width('100%').height('100%')
        .onSizeChange((_old: SizeOptions, size: SizeOptions) => {
          this.surface.setXComponentSurfaceSize({ surfaceWidth: Math.round(vp2px(Number(size.width))),
            surfaceHeight: Math.round(vp2px(Number(size.height))) });
          this.send({ op: 'redraw', id: this.windowId });
        })
        .onLoad(() => {
          this.surfaceId = this.surface.getXComponentSurfaceId();
          this.send({ op: 'attach', id: this.windowId, surface: this.surface.getXComponentSurfaceId(),
            density: this.getUIContext().vp2px(1) });
        })
        .onDestroy(() => this.aboutToDisappear())
      // A focusable leaf is required: an empty Column is a focus scope and
      // receives touch events but cannot retain hardware keyboard focus.
      Text('').id('slintInput').width('100%').height('100%').focusable(true).defaultFocus(true).focusOnTouch(true)
        .onBlur(() => this.keyboard.reset())
        .onTouch((event: TouchEvent) => {
          if (event.type === TouchType.Down) this.getUIContext().getFocusController().requestFocus('slintInput');
          if (event.source === SourceType.Mouse) return;
          if (event.changedTouches.length === 0) return;
          const p = event.changedTouches[0];
          const kind = event.type === TouchType.Down ? 0 : event.type === TouchType.Up ? 1 :
            event.type === TouchType.Cancel ? 3 : 2;
          this.send({ op: 'pointer', id: this.windowId, kind, x: p.x, y: p.y, button: 0 });
        })
        .onMouse((event: MouseEvent) => {
          if (event.action === MouseAction.Press) this.getUIContext().getFocusController().requestFocus('slintInput');
          const kind = event.action === MouseAction.Press ? 0 : event.action === MouseAction.Release ? 1 : 2;
          this.send({ op: 'pointer', id: this.windowId, kind, x: event.x, y: event.y,
            button: event.button === MouseButton.Right ? 1 : event.button === MouseButton.Middle ? 2 : 0 });
          event.stopPropagation();
        })
        .onHover((hovered: boolean) => {
          if (!hovered) this.send({ op: 'pointerExit', id: this.windowId });
        })
        .onKeyPreIme((event: KeyEvent) => this.keyboard.preIme(event, this.windowId, (reply: DesktopEvent) => this.send(reply)))
        .onKeyEvent((event: KeyEvent) => {
          this.send(KeyboardHost.event(event, this.windowId));
          event.stopPropagation();
        })
        .onAxisEvent((event: AxisEvent) => {
          this.send({ op: 'scroll', id: this.windowId, x: event.x, y: event.y,
            dx: -event.getHorizontalAxisValue(), dy: -event.getVerticalAxisValue() });
        })
    }.width('100%').height('100%').backgroundColor(Color.White)
  }
}
