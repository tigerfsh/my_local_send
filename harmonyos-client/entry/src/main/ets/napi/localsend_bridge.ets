import bridge from 'libentry.so';
import { FileInfo } from '../model/FileInfo';
import { DeviceInfo } from '../model/DeviceInfo';

/**
 * 封装 C++ NAPI 桥接层，供 ArkTS 页面调用共享 SDK 能力。
 * NAPI 层只暴露原始类型（string/number/boolean），
 * 对象统一以 JSON 字符串在两侧传递。
 */
export class LocalSendBridge {
  private static eventCb: ((json: string) => void) | null = null;

  static start(dataDir: string, tcpPort: number, deviceName: string): boolean {
    return bridge.start(dataDir, tcpPort, deviceName);
  }

  static stop(): void {
    bridge.stop();
  }

  static addFile(path: string): boolean {
    return bridge.addFile(path);
  }

  static removeFile(fileId: string): boolean {
    return bridge.removeFile(fileId);
  }

  static getFileList(): FileInfo[] {
    const raw: string = bridge.getFileListJson();
    if (!raw) {
      return [];
    }
    const arr = JSON.parse(raw) as Array<Record<string, string | number | boolean>>;
    const out: FileInfo[] = [];
    arr.forEach((item) => {
      out.push(FileInfo.fromJson(item));
    });
    return out;
  }

  static rescan(): boolean {
    return bridge.rescan();
  }

  static getDevices(): DeviceInfo[] {
    const raw: string = bridge.getDevicesJson();
    if (!raw) {
      return [];
    }
    const arr = JSON.parse(raw) as Array<Record<string, string | number | boolean>>;
    const out: DeviceInfo[] = [];
    arr.forEach((item) => {
      out.push(DeviceInfo.fromJson(item));
    });
    return out;
  }

  static requestPair(deviceId: string): boolean {
    return bridge.requestPair(deviceId);
  }

  static pairDevice(deviceId: string, accept: boolean): boolean {
    return bridge.pairDevice(deviceId, accept);
  }

  static removeDevice(deviceId: string): boolean {
    return bridge.removeDevice(deviceId);
  }

  static connectByIp(ip: string, port: number): boolean {
    return bridge.connectByIp(ip, port);
  }

  static getSettingsJson(): string {
    return bridge.getSettingsJson();
  }

  static saveSettingsJson(json: string): void {
    bridge.saveSettingsJson(json);
  }

  static getPinned(): boolean {
    return bridge.getPinned();
  }

  static setPinned(on: boolean): boolean {
    return bridge.setPinned(on);
  }

  static getCacheDir(): string {
    return bridge.getCacheDir();
  }

  static setCacheDir(dir: string): boolean {
    return bridge.setCacheDir(dir);
  }

  static readTextFile(path: string): string {
    return bridge.readTextFile(path);
  }

  static onEvent(cb: (json: string) => void): void {
    LocalSendBridge.eventCb = cb;
    bridge.onEvent((json: string) => {
      if (LocalSendBridge.eventCb) {
        LocalSendBridge.eventCb(json);
      }
    });
  }
}
