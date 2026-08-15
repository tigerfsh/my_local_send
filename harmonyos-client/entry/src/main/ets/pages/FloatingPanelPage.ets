import { promptAction } from '@kit.ArkUI';
import { picker, fileIo } from '@kit.CoreFileKit';
import { common } from '@kit.AbilityKit';
import { FloatingWindowMgr } from '../window/FloatingWindowMgr';
import { FileListViewModel } from '../viewmodel/FileListViewModel';
import { FileListItem } from '../components/FileListItem';
import { MenuDialog } from '../components/MenuDialog';
import { FileInfo } from '../model/FileInfo';
import { LocalSendBridge } from '../napi/localsend_bridge';

@Entry
@Component
struct FloatingPanelPage {
  vm: FileListViewModel = new FileListViewModel();
  @State files: FileInfo[] = [];
  @State multiSelect: boolean = false;
  @State selectedIds: string[] = [];
  @StorageLink('pinned') pinned: boolean = false;
  @StorageLink('filesTick') @Watch('onFilesTick') filesTick: number = 0;

  private menuDialogController: CustomDialogController = new CustomDialogController({
    builder: MenuDialog({
      onMultiSelect: () => {
        this.multiSelect = !this.multiSelect;
        if (!this.multiSelect) {
          this.selectedIds = [];
        }
      },
      onSettings: () => {
        FloatingWindowMgr.showSettings();
      },
      onDevices: () => {
        FloatingWindowMgr.showDevices();
      },
      onClose: () => {
        FloatingWindowMgr.showBar();
      }
    }),
    alignment: DialogAlignment.TopEnd
  });

  aboutToAppear(): void {
    this.refreshFiles();
    this.pinned = LocalSendBridge.getPinned();
    // 5 秒无操作自动回收至收缩态（钉住时不回收）
    setTimeout(() => {
      if (!this.pinned && !this.multiSelect) {
        FloatingWindowMgr.showBar();
      }
    }, 5000);
  }

  private onFilesTick(): void {
    this.refreshFiles();
  }

  private refreshFiles(): void {
    this.vm.refresh();
    this.files = this.vm.files;
    // 清理已删除文件的选中态
    const stale: string[] = [];
    this.selectedIds.forEach((id: string) => {
      let found = false;
      this.files.forEach((f: FileInfo) => {
        if (f.fileId === id) {
          found = true;
        }
      });
      if (!found) {
        stale.push(id);
      }
    });
    if (stale.length > 0) {
      const keep: string[] = [];
      this.selectedIds.forEach((id: string) => {
        if (stale.indexOf(id) < 0) {
          keep.push(id);
        }
      });
      this.selectedIds = keep;
    }
  }

  private togglePinned(): void {
    this.pinned = !this.pinned;
    LocalSendBridge.setPinned(this.pinned);
  }

  private toggleSelect(fileId: string): void {
    if (this.selectedIds.indexOf(fileId) >= 0) {
      const next: string[] = [];
      this.selectedIds.forEach((id: string) => {
        if (id !== fileId) {
          next.push(id);
        }
      });
      this.selectedIds = next;
    } else {
      this.selectedIds = this.selectedIds.concat([fileId]);
    }
  }

  private selectAll(): void {
    const all: string[] = [];
    this.files.forEach((f: FileInfo) => {
      all.push(f.fileId);
    });
    this.selectedIds = all;
  }

  private deleteSelected(): void {
    this.selectedIds.forEach((id: string) => {
      LocalSendBridge.removeFile(id);
    });
    this.selectedIds = [];
    this.refreshFiles();
  }

  private deleteFile(fileId: string): void {
    LocalSendBridge.removeFile(fileId);
    this.refreshFiles();
  }

  private async pickFiles(): Promise<void> {
    let context = getContext(this) as common.UIAbilityContext;
    let documentPicker = new picker.DocumentViewPicker(context);
    try {
      let result = await documentPicker.select({ maxSelectNumber: 50 });
      let paths: string[] = [];
      for (let i = 0; i < result.length; i++) {
        let uri = result[i];
        try {
          let file = fileIo.openSync(uri, fileIo.OpenMode.READ_ONLY);
          let dest = context.filesDir + '/picker/' + Date.now() + '_' + i;
          fileIo.copyFileSync(file.fd, dest);
          fileIo.closeSync(file);
          paths.push(dest);
        } catch (e) {
          console.error('copy picker file failed: ' + JSON.stringify(e));
        }
      }
      this.vm.addFiles(paths);
      this.refreshFiles();
    } catch (e) {
      console.error('select failed: ' + JSON.stringify(e));
    }
  }

  private previewFile(path: string, type: string): void {
    AppStorage.setOrCreate<string>('previewPath', path);
    AppStorage.setOrCreate<string>('previewType', type);
    FloatingWindowMgr.showPreview();
  }

  // 文档/其它文件：文本走内置预览，非文本给出提示（可用 filePreview 接入系统预览）
  private openFile(path: string, fileName: string): void {
    const text = LocalSendBridge.readTextFile(path);
    if (text.length > 0) {
      AppStorage.setOrCreate<string>('previewText', text);
      AppStorage.setOrCreate<string>('previewName', fileName);
      this.previewFile(path, 'text');
      return;
    }
    promptAction.showToast({ message: '该文件类型暂不支持预览，请在文件管理器中查看' });
  }

  build() {
    Column() {
      // 标题栏：钉住 + ⋮
      Row() {
        Text('中转站')
          .fontSize(14)
          .fontWeight(FontWeight.Bold)
          .fontColor('#2B3A67')
        Blank()
        Button(this.pinned ? '📌' : '📍')
          .fontSize(14)
          .type(ButtonType.Normal)
          .opacity(this.pinned ? 1 : 0.4)
          .onClick(() => {
            this.togglePinned();
          })
        Button('⋮')
          .fontSize(14)
          .type(ButtonType.Normal)
          .onClick(() => {
            this.menuDialogController.open();
          })
      }
      .width('100%')
      .height(40)
      .padding({ left: 12, right: 6 })

      // 文件列表
      List({ space: 2 }) {
        ForEach(this.files, (file: FileInfo) => {
          ListItem() {
            FileListItem({
              file: file,
              multiSelect: this.multiSelect,
              selected: this.selectedIds.indexOf(file.fileId) >= 0,
              onToggle: (fileId: string) => {
                this.toggleSelect(fileId);
              },
              onPreview: (path: string, type: string) => {
                this.previewFile(path, type);
              },
              onOpen: (path: string, fileName: string) => {
                this.openFile(path, fileName);
              },
              onDelete: (fileId: string) => {
                this.deleteFile(fileId);
              }
            })
          }
        }, (file: FileInfo) => file.fileId)
      }
      .width('100%')
      .layoutWeight(1)
      .scrollBar(BarState.Auto)

      // 底部操作栏
      Row({ space: 10 }) {
        if (!this.multiSelect) {
          Button('＋ 添加文件')
            .fontSize(13)
            .type(ButtonType.Normal)
            .onClick(() => {
              this.pickFiles();
            })
        } else {
          Button('全选')
            .fontSize(13)
            .type(ButtonType.Normal)
            .onClick(() => {
              this.selectAll();
            })
          Button('批量删除 (' + this.selectedIds.length + ')')
            .fontSize(13)
            .fontColor(Color.White)
            .backgroundColor('#E5484D')
            .onClick(() => {
              this.deleteSelected();
            })
        }
      }
      .width('100%')
      .height(44)
      .justifyContent(FlexAlign.Center)
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#F7F8FA')
    .borderRadius(12)
  }
}
