import { FloatingWindowMgr } from '../window/FloatingWindowMgr';
import { FileListViewModel } from '../viewmodel/FileListViewModel';

@Entry
@Component
struct FloatingBarPage {
  vm: FileListViewModel = new FileListViewModel();
  @StorageLink('filesTick') @Watch('onFilesTick') filesTick: number = 0;

  aboutToAppear(): void {
    this.vm.refresh();
  }

  private onFilesTick(): void {
    this.vm.refresh();
  }

  build() {
    Row({ space: 8 }) {
      Text('中转站')
        .fontSize(14)
        .fontWeight(FontWeight.Bold)
        .fontColor(Color.White)
        .margin({ left: 14 })

      Blank()

      Text('⋮')
        .fontSize(16)
        .fontColor('#9FB0E0')
        .margin({ right: 10 })
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#2B3A67')
    .borderRadius(12)
    .onClick(() => {
      FloatingWindowMgr.showPanel();
    })
    .overlay(
      // 右上角红色文件数角标
      Stack() {
        if (this.vm.files.length > 0) {
          Text(this.vm.files.length > 99 ? '99+' : `${this.vm.files.length}`)
            .fontSize(10)
            .fontColor(Color.White)
            .fontWeight(FontWeight.Bold)
            .textAlign(TextAlign.Center)
            .width(20)
            .height(20)
            .borderRadius(10)
            .backgroundColor('#E5484D')
        }
      }
      .align(Alignment.TopEnd)
      .margin({ top: -6, right: -6 })
    )
  }
}
