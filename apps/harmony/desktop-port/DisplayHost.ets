import { display, window } from '@kit.ArkUI';

export interface DesktopDisplay {
  id: number;
  name: string;
  primary: boolean;
  x: number;
  y: number;
  width: number;
  height: number;
  density: number;
  usableX: number;
  usableY: number;
  usableWidth: number;
  usableHeight: number;
}

export class DisplayHost {
  private static revision: number = 0;
  private static observed: Map<number, display.Display> = new Map();

  static screen(target: window.Window): display.Display {
    return display.getDisplayByIdSync(target.getWindowProperties().displayId ?? display.getDefaultDisplaySync().id);
  }

  static global(target: window.Window): display.Position {
    const properties = target.getWindowProperties();
    return display.convertRelativeToGlobalCoordinate({
      displayId: properties.displayId ?? display.getDefaultDisplaySync().id,
      position: { x: properties.windowRect.left, y: properties.windowRect.top }
    });
  }

  static async move(target: window.Window, x: number, y: number): Promise<void> {
    const point = display.convertGlobalToRelativeCoordinate({ x: Math.round(x), y: Math.round(y) });
    await target.moveWindowToAsync(point.position.x, point.position.y, { displayId: point.displayId });
  }

  static async snapshot(): Promise<DesktopDisplay[]> {
    const primary = display.getDefaultDisplaySync().id;
    const screens = await display.getAllDisplays();
    const result: DesktopDisplay[] = [];
    for (const screen of screens) {
      const origin = display.convertRelativeToGlobalCoordinate({ displayId: screen.id, position: { x: 0, y: 0 } });
      let area: display.Rect = { left: 0, top: 0, width: screen.width, height: screen.height };
      try { area = await screen.getAvailableArea(); }
      catch (error) { console.warn(`Slint available display area: ${error.message}`); }
      result.push({ id: screen.id, name: screen.name, primary: screen.id === primary,
        x: origin.x, y: origin.y, width: screen.width, height: screen.height, density: screen.densityPixels,
        usableX: origin.x + area.left, usableY: origin.y + area.top,
        usableWidth: area.width, usableHeight: area.height });
    }
    return result;
  }

  static observe(changed: (screens: DesktopDisplay[]) => void): void {
    const refresh = async (): Promise<void> => {
      const revision = ++DisplayHost.revision;
      try {
        const screens = await DisplayHost.snapshot();
        if (revision !== DisplayHost.revision) return;
        DisplayHost.observed.forEach((screen: display.Display, id: number) => {
          if (!screens.some((item: DesktopDisplay) => item.id === id)) {
            try { screen.off('availableAreaChange'); } catch (_) {}
            DisplayHost.observed.delete(id);
          }
        });
        screens.forEach((item: DesktopDisplay) => {
          if (DisplayHost.observed.has(item.id)) return;
          const screen = display.getDisplayByIdSync(item.id);
          try { screen.on('availableAreaChange', () => { refresh(); }); }
          catch (error) { console.warn(`Slint display area events: ${error.message}`); }
          DisplayHost.observed.set(item.id, screen);
        });
        changed(screens);
      } catch (error) { console.warn(`Slint display refresh: ${error.message}`); }
    };
    display.on('add', () => { refresh(); });
    display.on('remove', () => { refresh(); });
    display.on('change', () => { refresh(); });
    refresh();
  }
}
