import { statusBarManager } from '@kit.DeskTopExtensionKit';
import { common } from '@kit.AbilityKit';
import { image } from '@kit.ImageKit';
import { emitter } from '@kit.BasicServicesKit';
import type { DesktopEvent } from './DesktopHost';

export class TrayHost {
  private static active: boolean = false;
  private static icon: image.PixelMap | undefined;
  static async create(context: common.UIAbilityContext, event: DesktopEvent,
    send: (event: DesktopEvent) => void): Promise<boolean> {
    if (TrayHost.active) return true;
    if (!await statusBarManager.isStatusBarCapabilitySupported(context)) return false;
    const data = await context.resourceManager.getMediaContent($r('app.media.app_icon').id);
    const source = image.createImageSource(data.buffer as ArrayBuffer);
    try { TrayHost.icon = await source.createPixelMap({ desiredSize: { width: 48, height: 48 } }); }
    finally { source.release(); }
    const action = (code: string): void => send({ op: 'tray', text: code });
    statusBarManager.on('statusBarIconClick', () => {
      action('show');
      // A minimized main window cannot foreground its UIAbility with
      // showWindow alone. This is an explicit click on our system tray icon.
      context.startAbility({ bundleName: context.abilityInfo.bundleName,
        abilityName: context.abilityInfo.name, moduleName: context.abilityInfo.moduleName })
        .catch((error: Error) => console.error(`Slint tray restore: ${error.message}`));
    });
    statusBarManager.on('rightMenuClick', (event: emitter.EventData) => {
      const code: string | undefined = (event.data as Record<string, string> | undefined)?.['menuCode'];
      if (typeof code === 'string') action(code);
    });
    const item: statusBarManager.StatusBarItem = {
      icons: { white: TrayHost.icon, black: TrayHost.icon },
      hoverTips: event.title ?? 'CrossDesk',
      quickOperation: { abilityName: '', moduleName: 'entry', title: 'CrossDesk', height: 30 },
      statusBarGroupMenu: [[
        { title: event.show ?? '', menuCode: 'show', menuAction: { abilityName: 'EntryAbility', moduleName: 'entry', notifyOnly: true } },
        { title: event.settings ?? '', menuCode: 'settings', menuAction: { abilityName: 'EntryAbility', moduleName: 'entry', notifyOnly: true } }
      ]]
    };
    try {
      await new Promise<void>((resolve, reject) => statusBarManager.addToStatusBar(context, item,
        (error) => { if (error) reject(error); else resolve(); }));
      TrayHost.active = true;
      return true;
    } catch (error) { TrayHost.release(); throw new Error(error.message); }
  }
  static async remove(context: common.UIAbilityContext): Promise<void> {
    if (!TrayHost.active) return;
    try {
      await new Promise<void>((resolve, reject) => statusBarManager.removeFromStatusBar(context,
        (error) => { if (error) reject(error); else resolve(); }));
    } finally { TrayHost.active = false; TrayHost.release(); }
  }
  private static release(): void {
    statusBarManager.off('statusBarIconClick'); statusBarManager.off('rightMenuClick');
    TrayHost.icon?.release(); TrayHost.icon = undefined;
  }
}
