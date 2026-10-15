import { UIAbility } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { DesktopHost } from '../pages/DesktopHost';

export default class EntryAbility extends UIAbility {
  onPrepareToTerminate(): boolean { return DesktopHost.requestExit(); }
  onNewWant(): void { DesktopHost.restore(); }
  onForeground(): void { DesktopHost.foreground(); }
  onWindowStageCreate(stage: window.WindowStage): void {
    DesktopHost.start(stage, this.context);
  }
}
