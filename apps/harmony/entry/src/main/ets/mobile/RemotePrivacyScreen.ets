export interface PrivacyStatus {
  state: number; supported: boolean; input_block_supported: boolean;
  overlay_active: boolean; input_blocked: boolean; remote_paused: boolean;
  revision: number; reason: string;
}

export class RemotePrivacyScreen {
  snapshot?: PrivacyStatus;
  pending: boolean = false;
  requestId: number = 0;
  private requestRevision: number = 0;
  private receivedAt: number = 0;
  private requestedAt: number = 0;
  private failure: string = '';

  receive(s: PrivacyStatus, now: number): void {
    if (!Number.isInteger(s.state) || s.state < 0 || s.state > 5 ||
      !Number.isInteger(s.revision) || s.revision < 0 || s.revision > 0xffffffff ||
      typeof s.supported !== 'boolean' || typeof s.input_block_supported !== 'boolean' ||
      typeof s.overlay_active !== 'boolean' || typeof s.input_blocked !== 'boolean' ||
      typeof s.remote_paused !== 'boolean' || typeof s.reason !== 'string' || s.reason.includes('\0')) return;
    if (s.state === 3 && (!s.supported || !s.overlay_active || s.remote_paused)) return;
    if (this.snapshot && ((s.revision - this.snapshot.revision) | 0) < 0) return;
    this.snapshot = s; this.receivedAt = now; this.failure = '';
    if (this.pending && ((s.revision - this.requestRevision) | 0) > 0 && s.state !== 2 && s.state !== 4) this.pending = false;
  }
  active(): boolean {
    const s = this.snapshot;
    return s !== undefined && (s.overlay_active || s.remote_paused || s.state === 2 || s.state === 3 || s.state === 4);
  }
  fresh(now: number): boolean { return this.snapshot !== undefined && now - this.receivedAt <= 5000; }
  enabled(now: number): boolean {
    const s = this.snapshot;
    return s !== undefined && this.fresh(now) && s.state === 3 && s.overlay_active && !s.remote_paused;
  }
  buttonTitle(now: number): string {
    if (this.enabled(now)) return '隐私屏已开启';
    const s = this.snapshot;
    if (this.fresh(now) && s?.supported && s.state === 1 && !this.active()) return '隐私屏已关闭';
    return '隐私屏';
  }
  feedback(now: number): string {
    const s = this.snapshot;
    if (!this.failure && (this.pending || this.fresh(now) && (s?.state === 2 || s?.state === 4))) return '';
    if (!this.failure && !this.pending && this.fresh(now) && s?.supported &&
      (this.enabled(now) || s.state === 1 && !this.active())) return '';
    const detail = this.detail(now);
    return this.status(now) + (detail ? '：' + detail : '');
  }
  canToggle(now: number): boolean {
    const s = this.snapshot;
    return s !== undefined && !this.pending && this.fresh(now) && s.state !== 4 && (this.active() || s.supported && s.state === 1);
  }
  begin(now: number): number {
    this.tick(now);
    if (!this.canToggle(now) || !this.snapshot) return -1;
    this.pending = true; this.requestedAt = now; this.requestRevision = this.snapshot.revision; this.failure = '';
    this.requestId = (this.requestId + 1) >>> 0;
    return this.requestId;
  }
  failed(id: number): void {
    if (this.pending && this.requestId === id) { this.pending = false; this.failure = '指令发送失败，请重试'; }
  }
  tick(now: number): void {
    if (this.pending && now - this.requestedAt >= 15000) {
      this.pending = false; this.failure = '请求超时，尚未确认电脑的隐私屏状态';
    }
  }
  status(now: number): string {
    if (this.failure) return this.failure;
    const s = this.snapshot;
    if (!s) return '等待电脑返回隐私屏状态；旧版电脑端可能不支持';
    if (!this.fresh(now)) return '隐私屏状态已过期，等待电脑确认';
    if (s.state === 2) return '正在开启隐私屏…';
    if (s.state === 4) return '正在关闭隐私屏…';
    if (this.pending) return '指令已发送，等待电脑确认…';
    if (s.state === 5) return '隐私屏异常';
    if (!s.supported || s.state === 0) return '隐私屏暂不可用';
    if (s.state === 3 && s.overlay_active && !s.remote_paused)
      return '隐私屏已开启';
    return s.state === 1 && !this.active() ? '隐私屏已关闭' : '隐私屏状态待确认';
  }
  detail(now: number): string {
    const s = this.snapshot;
    return s && this.fresh(now) && (s.state === 0 || s.state === 5 || !s.supported) ? s.reason : '';
  }
}
