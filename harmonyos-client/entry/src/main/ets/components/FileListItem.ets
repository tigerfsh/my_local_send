import { FileInfo } from '../model/FileInfo';

function formatSize(bytes: number): string {
  if (bytes < 1024) {
    return bytes + ' B';
  }
  if (bytes < 1024 * 1024) {
    return (bytes / 1024).toFixed(1) + ' KB';
  }
  if (bytes < 1024 * 1024 * 1024) {
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
  }
  return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB';
}

@Component
export struct FileListItem {
  @Prop file: FileInfo = new FileInfo();
  @Prop multiSelect: boolean = false;
  @Prop selected: boolean = false;
  onToggle: (fileId: string) => void = () => {
  };
  onPreview: (path: string, type: string) => void = () => {
  };
  onDelete: (fileId: string) => void = () => {
  };

  build() {
    Row({ space: 8 }) {
      if (this.multiSelect) {
        Checkbox()
          .select(this.selected)
          .onChange((value: boolean) => {
            this.onToggle(this.file.fileId);
          })
      }

      // 缩略图 / 类型图标
      if (this.file.fileType === 1) {
        Image(this.file.cachePath)
          .width(36)
          .height(36)
          .borderRadius(6)
          .objectFit(ImageFit.Cover)
      } else {
        Column() {
          Text(this.file.fileType === 2 ? '▶' : '▤')
            .fontSize(16)
            .fontColor('#5A6B8C')
        }
        .width(36)
        .height(36)
        .justifyContent(FlexAlign.Center)
        .backgroundColor(this.file.fileType === 2 ? '#FFE7D9' : '#E8EAF0')
        .borderRadius(6)
      }

      Column({ space: 2 }) {
        Text(this.file.fileName)
          .fontSize(13)
          .fontColor('#22304A')
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
        Text(formatSize(this.file.fileSize))
          .fontSize(11)
          .fontColor('#8A93A6')
      }
      .alignItems(HorizontalAlign.Start)
      .layoutWeight(1)

      if (this.file.fileType === 1 || this.file.fileType === 2) {
        Button(this.file.fileType === 2 ? '播放' : '预览')
          .fontSize(12)
          .height(28)
          .type(ButtonType.Normal)
          .onClick(() => {
            this.onPreview(this.file.cachePath, this.file.fileType === 2 ? 'video' : 'image');
          })
      }

      if (!this.multiSelect) {
        Button('✕')
          .fontSize(12)
          .height(28)
          .type(ButtonType.Normal)
          .onClick(() => {
            this.onDelete(this.file.fileId);
          })
      }
    }
    .width('100%')
    .padding({ left: 6, right: 6, top: 8, bottom: 8 })
    .backgroundColor(this.selected ? '#E8EDFB' : Color.Transparent)
    .borderRadius(8)
  }
}
