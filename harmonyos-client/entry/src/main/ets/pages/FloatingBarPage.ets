import { FloatingWindowMgr } from '../window/FloatingWindowMgr';

/**
 * 收缩态：传送门图标（竖长椭圆），与 Ubuntu 版保持一致。
 * 点击进入「横向展开」堆叠态。
 */
@Entry
@Component
struct FloatingBarPage {
  build() {
    Stack() {
      // 外环 + 浅藏青实心
      Ellipse({ width: 40, height: 88 })
        .fill('#E8EEF8')
        .stroke('#2B3A67')
        .strokeWidth(7)
      // 内环
      Ellipse({ width: 24, height: 68 })
        .fillOpacity(0)
        .stroke('#7C8BB8')
        .strokeWidth(3)
      // 中心暖米小点
      Ellipse({ width: 7, height: 16 })
        .fill('#D6CDB9')
    }
    .width('100%')
    .height('100%')
    .alignContent(Alignment.End)
    .onClick(() => {
      FloatingWindowMgr.showDeck();
    })
  }
}
