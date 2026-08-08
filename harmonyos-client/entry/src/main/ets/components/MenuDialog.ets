import { CustomDialogController } from '@kit.ArkUI';

@CustomDialog
export struct MenuDialog {
  controller: CustomDialogController | null = null;
  onMultiSelect: () => void = () => {
  };
  onSettings: () => void = () => {
  };
  onDevices: () => void = () => {
  };
  onClose: () => void = () => {
  };

  build() {
    Column({ space: 4 }) {
      Button('多选')
        .fontSize(14)
        .type(ButtonType.Normal)
        .width('100%')
        .onClick(() => {
          this.onMultiSelect();
          this.controller?.close();
        })
      Button('设备管理')
        .fontSize(14)
        .type(ButtonType.Normal)
        .width('100%')
        .onClick(() => {
          this.onDevices();
          this.controller?.close();
        })
      Button('设置')
        .fontSize(14)
        .type(ButtonType.Normal)
        .width('100%')
        .onClick(() => {
          this.onSettings();
          this.controller?.close();
        })
      Button('关闭')
        .fontSize(14)
        .type(ButtonType.Normal)
        .width('100%')
        .onClick(() => {
          this.onClose();
          this.controller?.close();
        })
    }
    .padding(12)
    .backgroundColor(Color.White)
    .borderRadius(12)
    .width(160)
  }
}
