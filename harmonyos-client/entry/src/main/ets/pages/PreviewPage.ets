import { FloatingWindowMgr } from '../window/FloatingWindowMgr';

@Entry
@Component
struct PreviewPage {
  @State path: string = '';
  @State type: string = 'image';
  private videoController: VideoController = new VideoController();

  aboutToAppear(): void {
    this.path = AppStorage.get<string>('previewPath') ?? '';
    this.type = AppStorage.get<string>('previewType') ?? 'image';
  }

  private close(): void {
    this.videoController.stop();
    FloatingWindowMgr.showPanel();
  }

  build() {
    Stack() {
      if (this.type === 'video') {
        Video({
          src: this.path,
          previewUri: '',
          controller: this.videoController
        })
          .width('100%')
          .height('88%')
          .autoPlay(true)
          .controls(true)
          .objectFit(ImageFit.Contain)
      } else {
        Image(this.path)
          .width('100%')
          .height('100%')
          .objectFit(ImageFit.Contain)
          .onClick(() => {
            this.close();
          })
      }

      // 关闭按钮
      Button('✕')
        .fontSize(16)
        .fontColor(Color.White)
        .backgroundColor('#66000000')
        .width(36)
        .height(36)
        .borderRadius(18)
        .position({ x: 12, y: 12 })
        .onClick(() => {
          this.close();
        })

      if (this.type !== 'video') {
        Text('点击任意位置关闭预览')
          .fontSize(12)
          .fontColor('#CCCCCC')
          .position({ x: 0, y: '80%' })
          .width('100%')
          .textAlign(TextAlign.Center)
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#F2000000')
  }
}
