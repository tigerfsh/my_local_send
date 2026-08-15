import { window, display, promptAction } from '@kit.ArkUI';
import { common } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

const TAG: string = 'FloatingWindowMgr';

/**
 * 悬浮窗管理：基于 window 子窗口能力创建置顶悬浮窗。
 * 三态：收缩(传送门) -> 横向展开(堆叠) -> 纵向展开(列表)。
 * 注意：需要 SYSTEM_FLOAT_WINDOW 权限（系统级 ACL 授权）。
 */
export class FloatingWindowMgr {
  private static floatWin: window.Window | null = null;
  private static ctx: common.UIAbilityContext | null = null;
  private static currentPage: string = 'pages/FloatingBarPage';
  private static winWidth: number = 44;
  private static winHeight: number = 96;
  // 悬浮窗距顶部距离（vp）
  private static marginTop: number = 100;

  static init(ctx: common.UIAbilityContext): void {
    FloatingWindowMgr.ctx = ctx;
  }

  static get isCreated(): boolean {
    return FloatingWindowMgr.floatWin !== null;
  }

  static async create(): Promise<void> {
    if (FloatingWindowMgr.floatWin) {
      await FloatingWindowMgr.floatWin.showWindow();
      return;
    }
    if (!FloatingWindowMgr.ctx) {
      return;
    }
    const config: window.Configuration = {
      name: 'floatWindow',
      windowType: window.WindowType.TYPE_FLOAT,
      ctx: FloatingWindowMgr.ctx
    };
    try {
      const win = await window.createWindow(config);
      FloatingWindowMgr.floatWin = win;
      await win.resize(FloatingWindowMgr.winWidth, FloatingWindowMgr.winHeight);
      await FloatingWindowMgr.dockRight();
      await win.setUIContent('pages/FloatingBarPage');
      await win.showWindow();
      FloatingWindowMgr.currentPage = 'pages/FloatingBarPage';
      hilog.info(0x0000, TAG, 'floating window created');
    } catch (err) {
      let e = err as BusinessError;
      hilog.error(0x0000, TAG, 'create floating window failed: %{public}s', JSON.stringify(e));
      promptAction.showToast({
        message: '悬浮窗创建失败，请检查是否已授予 SYSTEM_FLOAT_WINDOW 权限'
      });
    }
  }

  static async close(): Promise<void> {
    if (FloatingWindowMgr.floatWin) {
      await FloatingWindowMgr.floatWin.destroyWindow();
      FloatingWindowMgr.floatWin = null;
    }
  }

  // 收缩态：传送门
  static async showBar(): Promise<void> {
    await FloatingWindowMgr.loadPage('pages/FloatingBarPage', 44, 96);
  }

  // 横向展开态：堆叠 + 个数
  static async showDeck(): Promise<void> {
    await FloatingWindowMgr.loadPage('pages/DeckPage', 160, 96);
  }

  // 纵向展开态：完整文件列表
  static async showPanel(): Promise<void> {
    await FloatingWindowMgr.loadPage('pages/FloatingPanelPage', 344, 480);
  }

  static async showPreview(): Promise<void> {
    await FloatingWindowMgr.loadPage('pages/PreviewPage', 720, 520);
  }

  static async showSettings(): Promise<void> {
    await FloatingWindowMgr.loadPage('pages/SettingsPage', 360, 480);
  }

  static async showDevices(): Promise<void> {
    await FloatingWindowMgr.loadPage('pages/DeviceManagerPage', 360, 520);
  }

  static async setPosition(x: number, y: number): Promise<void> {
    if (FloatingWindowMgr.floatWin) {
      await FloatingWindowMgr.floatWin.moveWindowTo(x, y);
    }
  }

  // 贴屏幕右边缘
  private static async dockRight(): Promise<void> {
    if (!FloatingWindowMgr.floatWin) {
      return;
    }
    try {
      const d = display.getDefaultDisplaySync();
      const x = d.width - FloatingWindowMgr.winWidth;
      await FloatingWindowMgr.floatWin.moveWindowTo(x, FloatingWindowMgr.marginTop);
    } catch (err) {
      let e = err as BusinessError;
      hilog.error(0x0000, TAG, 'dockRight failed: %{public}s', JSON.stringify(e));
    }
  }

  private static async loadPage(page: string, w: number, h: number): Promise<void> {
    const win = FloatingWindowMgr.floatWin;
    if (!win) {
      return;
    }
    try {
      FloatingWindowMgr.winWidth = w;
      FloatingWindowMgr.winHeight = h;
      await win.resize(w, h);
      await FloatingWindowMgr.dockRight();
      await win.setUIContent(page);
      FloatingWindowMgr.currentPage = page;
    } catch (err) {
      let e = err as BusinessError;
      hilog.error(0x0000, TAG, 'loadPage %{public}s failed: %{public}s', page, JSON.stringify(e));
    }
  }

  static get windowWidth(): number {
    return FloatingWindowMgr.winWidth;
  }

  static get windowHeight(): number {
    return FloatingWindowMgr.winHeight;
  }

  static get page(): string {
    return FloatingWindowMgr.currentPage;
  }
}
