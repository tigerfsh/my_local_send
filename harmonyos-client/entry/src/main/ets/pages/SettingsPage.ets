import { FloatingWindowMgr } from '../window/FloatingWindowMgr';
import { LocalSendBridge } from '../napi/localsend_bridge';

interface SettingsData {
  floatWindowLocked: boolean;
  autoCollapseMs: number;
  cacheExpireHours: number;
  maxTransferRateBps: number;
}

@Entry
@Component
struct SettingsPage {
  @State autoCollapseMs: number = 5000;
  @State floatWindowLocked: boolean = false;
  @State cacheExpireHours: number = 0;
  @State maxRateKB: number = 0;
  @State cacheDir: string = '';

  aboutToAppear(): void {
    let raw = LocalSendBridge.getSettingsJson();
    let data = JSON.parse(raw) as SettingsData;
    this.autoCollapseMs = data.autoCollapseMs;
    this.floatWindowLocked = data.floatWindowLocked;
    this.cacheExpireHours = data.cacheExpireHours;
    this.maxRateKB = Math.floor(data.maxTransferRateBps / 1024);
    this.cacheDir = LocalSendBridge.getCacheDir();
  }

  private save(): void {
    LocalSendBridge.saveSettingsJson(JSON.stringify({
      floatWindowLocked: this.floatWindowLocked,
      autoCollapseMs: this.autoCollapseMs,
      cacheExpireHours: this.cacheExpireHours,
      maxTransferRateBps: this.maxRateKB * 1024
    }));
  }

  private parseIntSafe(v: string, fallback: number): number {
    const n = parseInt(v);
    return Number.isNaN(n) ? fallback : n;
  }

  build() {
    Column({ space: 16 }) {
      Row() {
        Text('设置')
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .fontColor('#2B3A67')
        Blank()
        Button('返回')
          .fontSize(13)
          .type(ButtonType.Normal)
          .onClick(() => {
            this.save();
            FloatingWindowMgr.showPanel();
          })
      }
      .width('100%')
      .padding({ left: 12, right: 6 })

      Row() {
        Text('暂存目录').fontSize(14)
        Blank()
        Text(this.cacheDir)
          .fontSize(11)
          .fontColor('#8A93A6')
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
          .width(180)
      }
      .width('100%')
      .padding({ left: 12, right: 12 })

      Row() {
        Text('自动收起时长(毫秒)').fontSize(14)
        Blank()
        TextInput({ text: this.autoCollapseMs.toString() })
          .type(InputType.Number)
          .width(100)
          .height(32)
          .onChange((v: string) => {
            this.autoCollapseMs = this.parseIntSafe(v, this.autoCollapseMs);
          })
      }
      .width('100%')
      .padding({ left: 12, right: 12 })

      Row() {
        Text('悬浮窗位置锁定').fontSize(14)
        Blank()
        Toggle({ type: ToggleType.Switch, isOn: this.floatWindowLocked })
          .onChange((on: boolean) => {
            this.floatWindowLocked = on;
          })
      }
      .width('100%')
      .padding({ left: 12, right: 12 })

      Row() {
        Text('缓存过期(小时，0=不过期)').fontSize(14)
        Blank()
        TextInput({ text: this.cacheExpireHours.toString() })
          .type(InputType.Number)
          .width(100)
          .height(32)
          .onChange((v: string) => {
            this.cacheExpireHours = this.parseIntSafe(v, this.cacheExpireHours);
          })
      }
      .width('100%')
      .padding({ left: 12, right: 12 })

      Row() {
        Text('传输限速(KB/s，0=不限)').fontSize(14)
        Blank()
        TextInput({ text: this.maxRateKB.toString() })
          .type(InputType.Number)
          .width(100)
          .height(32)
          .onChange((v: string) => {
            this.maxRateKB = this.parseIntSafe(v, this.maxRateKB);
          })
      }
      .width('100%')
      .padding({ left: 12, right: 12 })

      Button('保存设置')
        .fontSize(14)
        .backgroundColor('#2B3A67')
        .onClick(() => {
          this.save();
        })
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#F7F8FA')
    .padding({ top: 12 })
  }
}
