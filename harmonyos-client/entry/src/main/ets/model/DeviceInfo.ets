export enum DeviceType {
  Unknown = 0,
  HarmonyOS = 1,
  Ubuntu = 2
}

export class DeviceInfo {
  deviceId: string = '';
  deviceName: string = '';
  deviceType: DeviceType = DeviceType.Unknown;
  ip: string = '';
  tcpPort: number = 0;
  isTrusted: boolean = false;
  isOnline: boolean = false;

  static fromJson(json: Record<string, string | number | boolean>): DeviceInfo {
    let d = new DeviceInfo();
    d.deviceId = String(json['deviceId'] ?? '');
    d.deviceName = String(json['deviceName'] ?? '');
    let dt = String(json['deviceType'] ?? 'unknown');
    if (dt === 'harmonyos') {
      d.deviceType = DeviceType.HarmonyOS;
    } else if (dt === 'ubuntu') {
      d.deviceType = DeviceType.Ubuntu;
    }
    d.ip = String(json['ip'] ?? '');
    d.tcpPort = Number(json['tcpPort'] ?? 0);
    d.isTrusted = Boolean(json['isTrusted'] ?? false);
    d.isOnline = Boolean(json['isOnline'] ?? false);
    return d;
  }

  toJson(): string {
    return JSON.stringify({
      deviceId: this.deviceId,
      deviceName: this.deviceName,
      deviceType: this.deviceType,
      ip: this.ip,
      tcpPort: this.tcpPort,
      isTrusted: this.isTrusted,
      isOnline: this.isOnline
    });
  }
}
