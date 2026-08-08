import { window, promptAction } from '@kit.ArkUI';
import { common } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

const TAG: string = 'FloatingWindowMgr';

/**
 * 悬浮窗管理：基于 window 子窗口能力创建置顶悬浮窗。
 * 注意：需要 SYSTEM_FLOAT_WINDOW 权限（系统级 ACL 授权）。
 */
export class FloatingWindowMgr {
  private static floatWin: window.Window | null = null;
  private static ctx: common.UIAbilityContext | null = null;
  private static currentPage: string = 'pages/FloatingBarPage';
  private static winWidth: number = 148;
  private static winHeight: number = 46;
  private static posX: number = 0;
  private static posY: number = 100;

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
    const options: window.WindowOptions = {
      name: 'floatWindow',
      windowType: window.WindowType.WINDOW_TYPE_FLOAT,
      ctx: FloatingWindowMgr.ctx
    };
    try {
      const win = await window.createWindow('floatWindow', options);
      FloatingWindowMgr.floatWin = win;
      await win.moveWindowTo(FloatingWindowMgr.posX, FloatingWindowMgr.posY);
      await win.resize(FloatingWindowMgr.winWidth, FloatingWindowMgr.winHeight);
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
      await FloatingWindowMgr.floatWin.hideWindow();
      await FloatingWindowMgr.floatWin.destroyWindow();
      FloatingWindowMgr.floatWin = null;
    }
  }

  static async showBar(): Promise<void> {
    await FloatingWindowMgr.loadPage('pages/FloatingBarPage', 148, 46);
  }

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
    FloatingWindowMgr.posX = x;
    FloatingWindowMgr.posY = y;
    if (FloatingWindowMgr.floatWin) {
      await FloatingWindowMgr.floatWin.moveWindowTo(x, y);
    }
  }

  private static async loadPage(page: string, w: number, h: number): Promise<void> {
    const win = FloatingWindowMgr.floatWin;
    if (!win) {
      return;
    }
    try {
      await win.resize(w, h);
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
