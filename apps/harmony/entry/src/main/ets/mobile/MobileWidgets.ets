import { tr } from './Localization';
import { LengthMetricsUnit } from '@kit.ArkUI';

export class MobileTheme {
  static readonly background: string = '#F2F2F7';
  static readonly ink: string = '#1C1C1E';
  static readonly secondary: string = '#737378';
  static readonly line: string = '#E2E2E7';
  static readonly blue: string = '#2E7ADB';
  static readonly red: string = '#FF3B30';
  static readonly green: string = '#248A3D';
}
// Vector geometry follows Android MobileUi.Icon; no font-dependent emoji glyphs.
@Component
export struct MobileIcon {
  @Prop @Watch('draw') name: string = 'info';
  @Prop @Watch('draw') color: string = MobileTheme.ink;
  iconExtent: number = 22;
  private context: CanvasRenderingContext2D = new CanvasRenderingContext2D(new RenderingContextSettings(true), LengthMetricsUnit.PX);
  private paths(): string[] {
    switch (this.name) {
      case 'cursor': return ['M1 1L1 13L5 10L9 17L12 15L8 9L15 9Z'];
      case 'folder': return ["M2 19V5h7l3 3h10v11z"];
      case 'keyboard': return ["M5.0 5.0H19.0Q22.0 5.0 22.0 8.0V16.0Q22.0 19.0 19.0 19.0H5.0Q2.0 19.0 2.0 16.0V8.0Q2.0 5.0 5.0 5.0Z", "M6 9h.01M10 9h.01M14 9h.01M18 9h.01M6 12h.01M10 12h.01M14 12h.01M18 12h.01M7 16h10"];
      case 'envelope': return ["M4.0 5.0H20.0Q22.0 5.0 22.0 7.0V17.0Q22.0 19.0 20.0 19.0H4.0Q2.0 19.0 2.0 17.0V7.0Q2.0 5.0 4.0 5.0Z", "M3 6l9 7 9-7"];
      case 'settings': return ["M9.807,4.828L10.264,2.152L13.736,2.152L14.193,4.828L15.521,5.378L17.736,3.808L20.192,6.264L18.622,8.479L19.172,9.807L21.848,10.264L21.848,13.736L19.172,14.193L18.622,15.521L20.192,17.736L17.736,20.192L15.521,18.622L14.193,19.172L13.736,21.848L10.264,21.848L9.807,19.172L8.479,18.622L6.264,20.192L3.808,17.736L5.378,15.521L4.828,14.193L2.152,13.736L2.152,10.264L4.828,9.807L5.378,8.479L3.808,6.264L6.264,3.808L8.479,5.378Z", "M8.8 12.0a3.2 3.2 0 1 0 6.4 0a3.2 3.2 0 1 0 -6.4 0"];
      case 'refresh': return ["M19.5 8A8 8 0 1 0 20 15M20 3v6h-6"];
      case 'chart': return ["M3 3v18h19M7 17v-4M12 17V8M18 17V4"];
      case 'arrow': return ["M4 12h16M14 6l6 6-6 6"];
      case 'muted': return ["M3 9h4l5-5v16l-5-5H3zM16 9l6 6m0-6-6 6"];
      case 'history': return ["M5.5 5.5a8.5 8.5 0 1 1-1 12M3 4v6h6M12 7v6l4 2"];
      case 'unlock': return ["M7.0 10.0H17.0Q19.0 10.0 19.0 12.0V20.0Q19.0 22.0 17.0 22.0H7.0Q5.0 22.0 5.0 20.0V12.0Q5.0 10.0 7.0 10.0Z", "M8 10V6a4 4 0 0 1 7.5-2M12 15v2"];
      case 'check': return ["M5 12l4 4L19 6"];
      case 'info': return ["M3.0 12.0a9.0 9.0 0 1 0 18.0 0a9.0 9.0 0 1 0 -18.0 0", "M12 10v7M12 6h.01"];
      case 'display': return ["M4.0 3.0H20.0Q22.0 3.0 22.0 5.0V15.0Q22.0 17.0 20.0 17.0H4.0Q2.0 17.0 2.0 15.0V5.0Q2.0 3.0 4.0 3.0Z", "M12 17v4M7 21h10"];
      case 'lock': return ["M7.0 10.0H17.0Q19.0 10.0 19.0 12.0V20.0Q19.0 22.0 17.0 22.0H7.0Q5.0 22.0 5.0 20.0V12.0Q5.0 10.0 7.0 10.0Z", "M8 10V6a4 4 0 0 1 8 0v4M12 15v2"];
      case 'eye': return ["M2 12c5-9 15-9 20 0-5 9-15 9-20 0z", "M9.0 12.0a3.0 3.0 0 1 0 6.0 0a3.0 3.0 0 1 0 -6.0 0"];
      case 'mouse_left': return ["M5 12v-3a7 7 0 0 1 7-7v10z", "M12.0 2.0H12.0Q19.0 2.0 19.0 9.0V15.0Q19.0 22.0 12.0 22.0H12.0Q5.0 22.0 5.0 15.0V9.0Q5.0 2.0 12.0 2.0Z", "M12 2v10M5 12h14"];
      case 'sliders': return ["M3 6h3m4 0h11M3 12h10m4 0h4M3 18h3m4 0h11", "M6.0 6.0a2.0 2.0 0 1 0 4.0 0a2.0 2.0 0 1 0 -4.0 0", "M13.0 12.0a2.0 2.0 0 1 0 4.0 0a2.0 2.0 0 1 0 -4.0 0", "M6.0 18.0a2.0 2.0 0 1 0 4.0 0a2.0 2.0 0 1 0 -4.0 0"];
      case 'close': return ["M6 6l12 12M18 6L6 18"];
      case 'eye_off': return ["M3 3l18 18M10 5c5-1 9 3 12 7a21 21 0 0 1-4 5M6 6a22 22 0 0 0-4 6c5 8 11 9 16 5M10 10a3 3 0 0 0 4 4"];
      case 'chevron': return ["M9 5l7 7-7 7"];
      case 'back': return ["M15 4l-8 8 8 8"];
      case 'mouse': return ["M12.0 2.0H12.0Q19.0 2.0 19.0 9.0V15.0Q19.0 22.0 12.0 22.0H12.0Q5.0 22.0 5.0 15.0V9.0Q5.0 2.0 12.0 2.0Z", "M12 3v7"];
      case 'clipboard': return ["M8 4H6a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V6a2 2 0 0 0-2-2h-2", "M10.0 2.0H14.0Q16.0 2.0 16.0 4.0V5.0Q16.0 7.0 14.0 7.0H10.0Q8.0 7.0 8.0 5.0V4.0Q8.0 2.0 10.0 2.0Z"];
      case 'audio': return ["M3 9h4l5-5v16l-5-5H3zM17 6a8 8 0 0 1 0 12M15 9a4 4 0 0 1 0 6"];
      case 'mouse_right': return ["M12 2a7 7 0 0 1 7 7v3h-7z", "M12.0 2.0H12.0Q19.0 2.0 19.0 9.0V15.0Q19.0 22.0 12.0 22.0H12.0Q5.0 22.0 5.0 15.0V9.0Q5.0 2.0 12.0 2.0Z", "M12 2v10M5 12h14"];
      case 'key': return ["M2.0 8.0a5.0 5.0 0 1 0 10.0 0a5.0 5.0 0 1 0 -10.0 0", "M10.5 11.5L19 20m-4-4 3-3m0 6 3-3M6 7h.01"];
      default: return ['M12 10v7M12 6h.01'];
    }
  }
  private draw(): void {
    const ctx = this.context;
    // SVG path coordinates are pixels. Scale both geometry and stroke from the
    // 24-unit viewport into physical pixels, including the display density.
    const extent = this.getUIContext().vp2px(this.iconExtent);
    ctx.clearRect(0, 0, extent, extent); ctx.save(); ctx.scale(extent / 24, extent / 24);
    ctx.strokeStyle = this.color; ctx.fillStyle = this.color; ctx.lineWidth = 1.7; ctx.lineCap = 'round'; ctx.lineJoin = 'round';
    this.paths().forEach((data: string, index: number) => {
      const path = new Path2D(data, LengthMetricsUnit.PX);
      if (this.name === 'cursor') { ctx.fillStyle = '#FFFFFF'; ctx.lineWidth = 1.5; ctx.fill(path); }
      if (index === 0 && (this.name === 'mouse_left' || this.name === 'mouse_right')) ctx.fill(path);
      ctx.stroke(path);
    });
    ctx.restore();
  }
  build() {
    Canvas(this.context).width(this.iconExtent).height(this.iconExtent).onReady(() => this.draw())
      .hitTestBehavior(HitTestMode.None).accessibilityLevel('no')
  }
}
@Component
export struct MobileIconButton {
  @Prop icon: string = 'close';
  @Prop label: string = '';
  iconExtent: number = 40;
  iconSize: number = 22;
  color: string = MobileTheme.ink;
  buttonFill: string = '#FFFFFF';
  action: () => void = () => {};
  build() {
    Button({ type: ButtonType.Normal, stateEffect: true }) {
      MobileIcon({ name: this.icon, iconExtent: this.iconSize, color: this.color })
    }.width(this.iconExtent).height(this.iconExtent).padding(0).borderRadius(10).backgroundColor(this.buttonFill)
      .accessibilityText(this.label).onClick(this.action)
  }
}
@Component
export struct MobilePrimaryButton {
  label: string = tr("连接");
  @Prop available: boolean = true;
  arrow: boolean = false;
  buttonHeight: number = 48;
  action: () => void = () => {};
  build() {
    Button({ type: ButtonType.Normal, stateEffect: true }) {
      Row({ space: 8 }) {
        Text(this.label).fontSize(16).minFontSize(11).maxFontSize(16).maxLines(2).textAlign(TextAlign.Center).fontWeight(600).fontColor(Color.White)
        if (this.arrow) { MobileIcon({ name: 'arrow', iconExtent: 18, color: '#FFFFFF' }) }
      }.justifyContent(FlexAlign.Center).width('100%')
    }.height(this.buttonHeight).width('100%').padding({ left: 12, right: 12 }).borderRadius(12)
      .linearGradient({ angle: 135, colors: [[MobileTheme.blue, 0], ['#1F61C7', 1]] })
      .enabled(this.available).opacity(this.available ? 1 : 0.45).accessibilityText(this.label).onClick(this.action)
  }
}
@Component
export struct MobileSegment {
  labels: string[] = [];
  @Prop @Watch('selectionChanged') selection: number = 0;
  @Prop available: boolean = true;
  changed: (index: number) => void = () => {};
  @State private selected: number = 0;
  @State private viewWidth: number = 1;
  aboutToAppear(): void { this.selected = this.selection; }
  private selectionChanged(): void { this.getUIContext().animateTo({ duration: 200, curve: Curve.EaseInOut }, () => this.selected = this.selection); }
  build() {
    Stack({ alignContent: Alignment.Start }) {
      Row() {}.width(Math.max(0, this.viewWidth - 4) / Math.max(1, this.labels.length)).height(28)
        .backgroundColor(Color.White).borderRadius(7).shadow({ radius: 2, color: '#18000000', offsetY: 1 }).translate({ x: 2 + this.selected * Math.max(0, this.viewWidth - 4) / Math.max(1, this.labels.length) })
      Row() {
        ForEach(this.labels, (label: string, index: number) => {
          Text(label).fontSize(12).minFontSize(9).maxFontSize(12).maxLines(2).fontWeight(this.selection === index ? 600 : 400).fontColor(MobileTheme.ink)
            .textAlign(TextAlign.Center).height(32).layoutWeight(1).enabled(this.available)
            .accessibilityLevel('yes').accessibilityText(label).accessibilitySelected(this.selection === index)
            .onClick(() => this.changed(index))
        }, (label: string) => label)
      }.padding({ left: 2, right: 2 }).width('100%')
    }.height(32).width('100%').backgroundColor('#E9E9EE').borderRadius(9).opacity(this.available ? 1 : 0.5)
      .onAreaChange((_old: Area, next: Area) => this.viewWidth = Number(next.width))
  }
}
