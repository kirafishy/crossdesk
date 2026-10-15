import { autoStartupManager } from '@kit.AbilityKit';

export interface AutostartEvent {
  op: string;
  pending?: boolean;
  ok?: boolean;
  supported?: boolean;
  active?: boolean;
}

// Public APIs can only query the user's system setting. Never derive this
// state from an application preference, or treat a failed query as disabled.
export class AutostartHost {
  private static running: boolean = false;
  private static sequence: number = 0;
  private static timer: number = -1;
  private static notify: (event: AutostartEvent) => void = () => {};

  static start(notify: (event: AutostartEvent) => void): void {
    AutostartHost.stop();
    AutostartHost.notify = notify;
    AutostartHost.running = true;
    AutostartHost.refresh();
  }

  static stop(): void {
    AutostartHost.running = false;
    ++AutostartHost.sequence;
    clearTimeout(AutostartHost.timer);
    AutostartHost.timer = -1;
  }

  static refresh(): void {
    if (!AutostartHost.running) return;
    const sequence = ++AutostartHost.sequence;
    clearTimeout(AutostartHost.timer);
    AutostartHost.notify({ op: 'autostartState', pending: true });
    // A stalled system service must not leave the settings page loading forever.
    AutostartHost.timer = setTimeout(() => {
      AutostartHost.finish(sequence, { op: 'autostartState', ok: false });
    }, 5000);
    try {
      if (!autoStartupManager.isAutoStartupSupported()) {
        AutostartHost.finish(sequence, { op: 'autostartState', ok: true, supported: false });
        return;
      }
      autoStartupManager.getAutoStartupStatusForSelf().then((active: boolean) => {
        AutostartHost.finish(sequence, { op: 'autostartState', ok: true, supported: true, active });
      }).catch((error: Error) => {
        console.warn(`Slint autostart query: ${error.message}`);
        AutostartHost.finish(sequence, { op: 'autostartState', ok: false });
      });
    } catch (error) {
      console.warn(`Slint autostart capability: ${error.message}`);
      AutostartHost.finish(sequence, { op: 'autostartState', ok: false });
    }
  }

  private static finish(sequence: number, event: AutostartEvent): void {
    if (!AutostartHost.running || sequence !== AutostartHost.sequence) return;
    ++AutostartHost.sequence; // Also reject a result arriving after its timeout.
    clearTimeout(AutostartHost.timer);
    AutostartHost.timer = -1;
    AutostartHost.notify(event);
  }
}
