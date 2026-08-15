export enum FileType {
  Unknown = 0,
  Image = 1,
  Video = 2,
  Doc = 3,
  Other = 4
}

export class FileInfo {
  fileId: string = '';
  fileName: string = '';
  fileType: FileType = FileType.Unknown;
  fileSize: number = 0;
  duration: number = 0;
  cachePath: string = '';
  createTime: string = '';
  expireTime: string = '';
  isLocal: boolean = true;

  static fromJson(json: Record<string, string | number | boolean>): FileInfo {
    let fi = new FileInfo();
    fi.fileId = String(json['fileId'] ?? '');
    fi.fileName = String(json['fileName'] ?? '');
    let ft = String(json['fileType'] ?? 'unknown');
    if (ft === 'image') {
      fi.fileType = FileType.Image;
    } else if (ft === 'video') {
      fi.fileType = FileType.Video;
    } else if (ft === 'doc') {
      fi.fileType = FileType.Doc;
    }
    fi.fileSize = Number(json['fileSize'] ?? 0);
    fi.duration = Number(json['fileDuration'] ?? 0);
    fi.cachePath = String(json['cachePath'] ?? '');
    fi.createTime = String(json['createTime'] ?? '');
    fi.isLocal = true;
    return fi;
  }

  toJson(): string {
    return JSON.stringify({
      fileId: this.fileId,
      fileName: this.fileName,
      fileType: this.fileType,
      fileSize: this.fileSize,
      fileDuration: this.duration,
      cachePath: this.cachePath,
      createTime: this.createTime,
      expireTime: this.expireTime
    });
  }
}
