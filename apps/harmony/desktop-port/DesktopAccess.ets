import { abilityAccessCtrl, common, Permissions } from '@kit.AbilityKit';
import { UIContext } from '@kit.ArkUI';
import { DesktopEvent } from './DesktopHost';

// OS authorization only; session policy stays in the shared desktop runtime.
export class DesktopAccess {
  private static context: common.UIAbilityContext;
  private static enabled: boolean = false;
  private static hostEnabled: boolean = false;
  private static hostRequested: boolean = false;
  private static running: boolean = false;
  private static generation: number = 0;
  private static requested: Set<Permissions> = new Set();
  private static requests: Promise<void> = Promise.resolve();
  private static notify: (event: DesktopEvent) => void = () => {};
  private static manager: abilityAccessCtrl.AtManager = abilityAccessCtrl.createAtManager();
  private static permissions: Permissions[] = [];
  private static changed: (info: abilityAccessCtrl.PermissionStateChangeInfo) => void = () => DesktopAccess.refresh();

  static start(context: common.UIAbilityContext, enabled: boolean, notify: (event: DesktopEvent) => void,
    hostEnabled: boolean = false): void {
    if (DesktopAccess.running) DesktopAccess.stop();
    DesktopAccess.context = context; DesktopAccess.enabled = enabled;
    DesktopAccess.hostEnabled = hostEnabled;
    DesktopAccess.hostRequested = false;
    DesktopAccess.running = true; DesktopAccess.notify = notify;
    ++DesktopAccess.generation; DesktopAccess.requested.clear();
    DesktopAccess.permissions = [];
    if (enabled) DesktopAccess.permissions.push('ohos.permission.HOOK_KEY_EVENT', 'ohos.permission.READ_PASTEBOARD');
    if (hostEnabled) DesktopAccess.permissions.push('ohos.permission.CUSTOM_SCREEN_RECORDING', 'ohos.permission.CONTROL_DEVICE');
    if (DesktopAccess.permissions.length > 0) {
      try { DesktopAccess.manager.on('selfPermissionStateChange', DesktopAccess.permissions, DesktopAccess.changed); }
      catch (error) { console.warn(`Slint permission observation: ${error.message}`); }
    }
    DesktopAccess.refresh();
  }

  static stop(): void {
    if (!DesktopAccess.running) return;
    DesktopAccess.notify({ op: 'hostPermissions', controlDevice: false, screenRecording: false });
    DesktopAccess.running = false;
    ++DesktopAccess.generation;
    if (DesktopAccess.permissions.length > 0) {
      try { DesktopAccess.manager.off('selfPermissionStateChange', DesktopAccess.permissions, DesktopAccess.changed); }
      catch (error) { console.warn(`Slint permission cleanup: ${error.message}`); }
    }
  }

  static granted(permission: Permissions): boolean {
    try {
      return DesktopAccess.manager.checkAccessTokenSync(DesktopAccess.context.applicationInfo.accessTokenId, permission) ===
        abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED;
    } catch { return false; }
  }

  static refresh(): void {
    if (!DesktopAccess.running) return;
    DesktopAccess.notify({ op: 'keyboardPermission', ok: DesktopAccess.granted('ohos.permission.HOOK_KEY_EVENT') });
    DesktopAccess.notify({ op: 'clipboardChanged' });
    DesktopAccess.notify({ op: 'hostPermissions',
      controlDevice: DesktopAccess.hostEnabled && DesktopAccess.granted('ohos.permission.CONTROL_DEVICE'),
      screenRecording: DesktopAccess.hostEnabled && DesktopAccess.granted('ohos.permission.CUSTOM_SCREEN_RECORDING') });
  }

  static requestKeyboard(): void {
    if (DesktopAccess.enabled) DesktopAccess.request('ohos.permission.HOOK_KEY_EVENT', true);
  }

  static requestClipboard(): void {
    if (DesktopAccess.enabled) DesktopAccess.request('ohos.permission.READ_PASTEBOARD', false);
  }

  static requestHost(ui: UIContext): void {
    if (!DesktopAccess.running || !DesktopAccess.hostEnabled || DesktopAccess.hostRequested) return;
    DesktopAccess.hostRequested = true;
    const generation = DesktopAccess.generation;
    DesktopAccess.requests = DesktopAccess.requests.then(async () => {
      if (!DesktopAccess.running || generation !== DesktopAccess.generation) return;
      const switches = DesktopAccess.missingHostSwitches();
      if (switches.length === 0) return;
      const prompt = ui.getPromptAction();
      const result = await prompt.showDialog({
        title: '设置远程访问权限',
        message: '请在接下来打开的应用设置页开启：\n\n' + switches.join('\n') +
          '\n\n仅用于已通过连接密码验证的远程会话。你可以随时在该页面关闭权限。',
        buttons: [{ text: '暂不开启', color: '#666666' }, { text: '前往设置', color: '#007DFF' }]
      });
      if (result.index !== 1 || !DesktopAccess.running || generation !== DesktopAccess.generation ||
        DesktopAccess.missingHostSwitches().length === 0) return;
      try {
        // The system manual-permission dialog describes a different settings
        // route on PC. Open this app's actual switches together instead.
        await DesktopAccess.context.startAbility({
          bundleName: 'com.huawei.hmos.settings',
          abilityName: 'com.huawei.hmos.settings.MainAbility',
          uri: 'application_info_entry',
          parameters: { pushParams: DesktopAccess.context.applicationInfo.name }
        });
      } catch (error) {
        if (!DesktopAccess.running || generation !== DesktopAccess.generation) return;
        console.warn(`Slint host permission settings: ${error.message}`);
        await prompt.showDialog({
          title: '未能打开应用设置',
          message: '请在系统设置的应用列表中找到本应用，再开启：\n\n' +
            DesktopAccess.missingHostSwitches().join('\n'),
          buttons: [{ text: '知道了', color: '#007DFF' }]
        });
      }
    }).catch((error: Error) => console.warn(`Slint host permission guide: ${error.message}`))
      .then(() => { if (generation === DesktopAccess.generation) DesktopAccess.refresh(); });
  }

  private static missingHostSwitches(): string[] {
    const switches: string[] = [];
    if (!DesktopAccess.granted('ohos.permission.CUSTOM_SCREEN_RECORDING'))
      switches.push('“屏幕共享”：允许远程设备查看本机屏幕。');
    if (!DesktopAccess.granted('ohos.permission.CONTROL_DEVICE'))
      switches.push('“辅助操作”：允许远程设备使用本机键盘和鼠标。');
    return switches;
  }

  private static request(permission: Permissions, manual: boolean): void {
    if (!DesktopAccess.running || DesktopAccess.requested.has(permission) || DesktopAccess.granted(permission)) return;
    DesktopAccess.requested.add(permission);
    const generation = DesktopAccess.generation;
    DesktopAccess.requests = DesktopAccess.requests.then(async () => {
      if (!DesktopAccess.running || generation !== DesktopAccess.generation || DesktopAccess.granted(permission)) return;
      if (manual) await DesktopAccess.manager.openPermissionOnSetting(DesktopAccess.context, permission);
      else await DesktopAccess.manager.requestPermissionsFromUser(DesktopAccess.context, [permission]);
    }).catch((error: Error) => console.warn(`Slint permission ${permission}: ${error.message}`))
      .then(() => { if (generation === DesktopAccess.generation) DesktopAccess.refresh(); });
  }
}
