import { FileInfo } from '../model/FileInfo';
import { LocalSendBridge } from '../napi/localsend_bridge';

/**
 * 文件列表视图模型：封装 NAPI 文件列表读取。
 * 多选/选中等交互状态由页面以 @State 维护（保证 UI 响应式刷新）。
 */
export class FileListViewModel {
  files: FileInfo[] = [];

  refresh(): void {
    this.files = LocalSendBridge.getFileList();
  }

  addFiles(paths: string[]): void {
    paths.forEach((p) => {
      LocalSendBridge.addFile(p);
    });
    this.refresh();
  }
}
