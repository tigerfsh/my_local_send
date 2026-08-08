import { FloatingWindowMgr } from '../window/FloatingWindowMgr';
import { LocalSendBridge } from '../napi/localsend_bridge';

@Entry
@Component
struct HomePage {
  @State fileCount: number = 0;
  @StorageLink('filesTick') @Watch('onTick') filesTick: number = 0;

  aboutToAppear(): void {
    this.refresh();
  }

  private onTick(): void {
    this.refresh();
  }

  private refresh(): void {
    this.fileCount = LocalSendBridge.getFileList().length;
  }

  build() {
    Column({ space: 16 }) {
      Text('文件中转站')
        .fontSize(24)
        .fontWeight(FontWeight.Bold)
        .fontColor('#2B3A67')

      Text('当前站内文件：' + this.fileCount)
        .fontSize(14)
        .fontColor('#44506B')

      Text('悬浮窗已常驻屏幕右侧，点击「中转站」悬浮条即可展开文件面板。\n' +
        '在悬浮窗「设备管理」中配对局域网设备后，可双向同步文件。')
        .fontSize(14)
        .fontColor('#44506B')
        .lineHeight(22)
        .textAlign(TextAlign.Center)
        .width('80%')

      Button('打开悬浮窗')
        .fontSize(14)
        .backgroundColor('#2B3A67')
        .onClick(() => {
          FloatingWindowMgr.create();
          FloatingWindowMgr.showPanel();
        })

      Button('最小化到后台')
        .fontSize(14)
        .type(ButtonType.Normal)
        .onClick(() => {
          FloatingWindowMgr.create();
        })
    }
    .width('100%')
    .height('100%')
    .justifyContent(FlexAlign.Center)
    .backgroundColor('#F7F8FA')
  }
}
