import { UIAbility } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { MobileLifecycle } from './MobilePlatform';

export default class EntryAbility extends UIAbility {
  onWindowStageCreate(stage: window.WindowStage): void {
    stage.loadContent('pages/Mobile', (error) => {
      if (error.code) hilog.error(0, 'CrossDesk', 'Unable to load the main page: %{public}d', error.code);
    });
  }
  onBackground(): void { MobileLifecycle.background(); }
  onForeground(): void { MobileLifecycle.foreground(); }
  onDestroy(): void { MobileLifecycle.destroy(); }
}
