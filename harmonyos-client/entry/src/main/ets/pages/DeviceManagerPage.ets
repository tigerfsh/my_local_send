import { FloatingWindowMgr } from '../window/FloatingWindowMgr';
import { LocalSendBridge } from '../napi/localsend_bridge';
import { DeviceInfo } from '../model/DeviceInfo';

@Entry
@Component
struct DeviceManagerPage {
  @State devices: DeviceInfo[] = [];
  @State ipInput: string = '';
  @State portInput: string = '53318';
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

  private connectByIp(): void {
    if (this.ipInput.trim() === '') {
      return;
    }
    const port = parseInt(this.portInput);
    if (Number.isNaN(port)) {
      return;
    }
    LocalSendBridge.connectByIp(this.ipInput.trim(), port);
    this.refresh();
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
      .height(300)
      .scrollBar(BarState.Auto)

      Row({ space: 8 }) {
        TextInput({ text: this.ipInput, placeholder: '设备 IP' })
          .width(120)
          .height(32)
          .onChange((v: string) => {
            this.ipInput = v;
          })
        TextInput({ text: this.portInput, placeholder: '端口' })
          .width(80)
          .height(32)
          .type(InputType.Number)
          .onChange((v: string) => {
            this.portInput = v;
          })
        Button('手动连接')
          .fontSize(12)
          .type(ButtonType.Normal)
          .onClick(() => {
            this.connectByIp();
          })
      }
      .width('100%')
      .padding({ left: 12, right: 12 })

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
    .padding({ top: 12 })
  }
}
