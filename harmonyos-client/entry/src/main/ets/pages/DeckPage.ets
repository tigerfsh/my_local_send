import { FloatingWindowMgr } from '../window/FloatingWindowMgr';
import { FileListViewModel } from '../viewmodel/FileListViewModel';
import { FileInfo, FileType } from '../model/FileInfo';

/**
 * 横向展开态：前 3 个文件封面上下交错堆叠 + 文件个数徽章。
 * 点击进入纵向文件列表。
 */
@Entry
@Component
struct DeckPage {
  vm: FileListViewModel = new FileListViewModel();
  @State files: FileInfo[] = [];
  @StorageLink('filesTick') @Watch('onFilesTick') filesTick: number = 0;

  aboutToAppear(): void {
    this.refresh();
  }

  private onFilesTick(): void {
    this.refresh();
  }

  private refresh(): void {
    this.vm.refresh();
    this.files = this.vm.files;
  }

  @Builder
  thumb(file: FileInfo, index: number) {
    if (file.fileType === FileType.Image) {
      Image(file.cachePath)
        .width(44)
        .height(44)
        .borderRadius(8)
        .objectFit(ImageFit.Cover)
        .border({ width: 2, color: Color.White })
        .translate({ x: 16 + index * 8, y: (index % 2 === 0 ? 6 : 20) })
    } else {
      Column() {
        Text(file.fileType === FileType.Video ? '▶' : '▤')
          .fontSize(16)
          .fontColor('#5A6B8C')
      }
      .width(44)
      .height(44)
      .justifyContent(FlexAlign.Center)
      .borderRadius(8)
      .backgroundColor(file.fileType === FileType.Video ? '#FFE7D9' : '#E8EAF0')
      .border({ width: 2, color: Color.White })
      .translate({ x: 16 + index * 8, y: (index % 2 === 0 ? 6 : 20) })
    }
  }

  build() {
    Row() {
      // 前 3 个封面，横向重叠 + 纵向交错
      Stack({ alignContent: Alignment.TopStart }) {
        ForEach(this.files.slice(0, 3), (file: FileInfo, index: number) => {
          this.thumb(file, index)
        }, (file: FileInfo) => file.fileId)
      }
      .width(76)
      .height(76)

      Blank()

      // 文件个数徽章
      Text(this.files.length > 99 ? '99+' : `${this.files.length}`)
        .fontSize(15)
        .fontWeight(FontWeight.Bold)
        .fontColor(Color.White)
        .textAlign(TextAlign.Center)
        .width(46)
        .height(46)
        .borderRadius(23)
        .backgroundColor('#2B3A67')
        .margin({ right: 14 })
    }
    .width('100%')
    .height('100%')
    .padding({ left: 14 })
    .backgroundColor('#F7F8FA')
    .borderRadius(12)
    .onClick(() => {
      FloatingWindowMgr.showPanel();
    })
  }
}
