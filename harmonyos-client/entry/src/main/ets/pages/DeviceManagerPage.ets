import { FloatingWindowMgr } from '../window/FloatingWindowMgr';
import { LocalSendBridge } from '../napi/localsend_bridge';
import { DeviceInfo } from '../model/DeviceInfo';

/**
 * 设备管理：自动扫描同一局域网设备，手动选择配对（无手动输 IP）。
 */
@Entry
@Component
struct DeviceManagerPage {
  @State devices: DeviceInfo[] = [];
  @StorageLink('devicesTick') @Watch('onDevicesTick') devicesTick: number = 0;

  aboutToAppear(): void {
    this.refresh();
  }

  private onDevicesTick(): void {
    this.refresh();
  }

  private refresh(): void {
    this.devices = LocalSendBridge.getDevices();
  }

  build() {
    Column({ space: 12 }) {
      Row() {
        Text('设备管理')
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .fontColor('#2B3A67')
        Blank()
        Button('返回')
          .fontSize(13)
          .type(ButtonType.Normal)
          .onClick(() => {
            FloatingWindowMgr.showPanel();
          })
      }
      .width('100%')
      .padding({ left: 12, right: 6 })

      Text('同一局域网的设备会自动出现在下方')
        .fontSize(11)
        .fontColor('#8A93A6')
        .width('100%')
        .padding({ left: 12, right: 12 })

      if (this.devices.length === 0) {
        Text('正在扫描局域网设备…')
          .fontSize(12)
          .fontColor('#8A93A6')
          .width('100%')
          .textAlign(TextAlign.Center)
          .margin({ top: 40 })
      }

      List({ space: 4 }) {
        ForEach(this.devices, (d: DeviceInfo) => {
          ListItem() {
            Row({ space: 8 }) {
              Column({ space: 2 }) {
                Text(d.deviceName)
                  .fontSize(13)
                  .fontColor('#22304A')
                  .maxLines(1)
                Text(d.ip + ':' + d.tcpPort)
                  .fontSize(11)
                  .fontColor('#8A93A6')
              }
              .alignItems(HorizontalAlign.Start)
              .layoutWeight(1)

              Text(d.isOnline ? '● 在线' : '○ 离线')
                .fontSize(11)
                .fontColor(d.isOnline ? '#2E9E5B' : '#9AA3B5')

              if (d.isTrusted) {
                Button('解除信任')
                  .fontSize(11)
                  .type(ButtonType.Normal)
                  .onClick(() => {
                    LocalSendBridge.removeDevice(d.deviceId);
                    this.refresh();
                  })
              } else {
                Button('配对')
                  .fontSize(11)
                  .type(ButtonType.Normal)
                  .onClick(() => {
                    LocalSendBridge.requestPair(d.deviceId);
                    this.refresh();
                  })
              }
            }
            .width('100%')
            .padding(8)
            .backgroundColor('#F2F4F8')
            .borderRadius(8)
          }
        }, (d: DeviceInfo) => d.deviceId)
      }
      .width('100%')
      .layoutWeight(1)
      .scrollBar(BarState.Auto)

      Button('重新扫描')
        .fontSize(13)
        .type(ButtonType.Normal)
        .onClick(() => {
          LocalSendBridge.rescan();
          this.refresh();
        })
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#F7F8FA')
    .padding({ top: 12, bottom: 12 })
  }
}
