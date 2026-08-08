import { AbilityConstant, UIAbility, Want, common } from '@kit.AbilityKit';
import { window, promptAction } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { LocalSendBridge } from '../napi/localsend_bridge';
import { FloatingWindowMgr } from '../window/FloatingWindowMgr';

const TAG: string = 'EntryAbility';

interface SdkEvent {
  event: string;
  deviceId?: string;
  deviceName?: string;
  accepted?: boolean;
  fileName?: string;
  ok?: boolean;
  message?: string;
}

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(0x0000, TAG, 'onCreate');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    // 启动共享 SDK：数据目录使用应用沙箱 filesDir
    const ctx = this.context;
    const dataDir = ctx.filesDir + '/localsend';
    LocalSendBridge.start(dataDir, 53318, '我的手机');
    LocalSendBridge.onEvent((json: string) => {
      this.handleSdkEvent(json);
    });
    FloatingWindowMgr.init(ctx);

    AppStorage.setOrCreate<number>('filesTick', 0);
    AppStorage.setOrCreate<number>('devicesTick', 0);

    // 主窗口加载引导主页（悬浮窗独立加载悬浮条页面，避免重复显示）
    windowStage.loadContent('pages/HomePage', (err) => {
      if (err.code) {
        hilog.error(0x0000, TAG, 'loadContent failed: %{public}s', JSON.stringify(err));
        return;
      }
      // 主窗口就绪后创建悬浮窗
      FloatingWindowMgr.create();
    });
  }

  private handleSdkEvent(json: string): void {
    try {
      const evt = JSON.parse(json) as SdkEvent;
      hilog.info(0x0000, TAG, 'sdk event: %{public}s', json);
      const name = evt.event;

      if (name === 'fileAdded' || name === 'fileRemoved' || name === 'transferFinished') {
        AppStorage.setOrCreate<number>('filesTick',
          (AppStorage.get<number>('filesTick') ?? 0) + 1);
        if (name === 'transferFinished') {
          if (evt.ok) {
            promptAction.showToast({ message: '已同步: ' + (evt.fileName ?? '') });
          } else {
            promptAction.showToast({ message: '同步失败: ' + (evt.fileName ?? '') });
          }
        }
        return;
      }

      if (name === 'devicesChanged' || name === 'pairResult') {
        AppStorage.setOrCreate<number>('devicesTick',
          (AppStorage.get<number>('devicesTick') ?? 0) + 1);
        if (name === 'pairResult') {
          promptAction.showToast({ message: evt.accepted ? '配对成功' : '配对已拒绝' });
        }
        return;
      }

      if (name === 'pairRequest') {
        const deviceId = evt.deviceId ?? '';
        const deviceName = evt.deviceName ?? '';
        AppStorage.setOrCreate<number>('devicesTick',
          (AppStorage.get<number>('devicesTick') ?? 0) + 1);
        promptAction.showDialog({
          title: '配对请求',
          message: '设备「' + deviceName + '」请求与本机配对，是否接受？',
          buttons: [
            { text: '拒绝', color: '#999999' },
            { text: '接受', color: '#2B3A67' }
          ]
        }).then((res) => {
          if (res.index === 1) {
            LocalSendBridge.pairDevice(deviceId, true);
          } else {
            LocalSendBridge.pairDevice(deviceId, false);
          }
        }).catch((err: Error) => {
          hilog.error(0x0000, TAG, 'showDialog failed: %{public}s', JSON.stringify(err));
        });
        return;
      }

      if (name === 'error') {
        promptAction.showToast({ message: evt.message ?? '发生错误' });
      }
    } catch (e) {
      hilog.error(0x0000, TAG, 'handleSdkEvent parse failed: %{public}s', JSON.stringify(e));
    }
  }

  onWindowStageDestroy(): void {
    LocalSendBridge.stop();
    FloatingWindowMgr.close();
  }

  onForeground(): void {
  }

  onBackground(): void {
  }
}
