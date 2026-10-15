import { common } from '@kit.AbilityKit';
import { picker, fileIo, fileUri, fileShare } from '@kit.CoreFileKit';
import { preferences } from '@kit.ArkData';
import { window } from '@kit.ArkUI';

// Only URI grants and native file access live here. Transfer/progress/cancel
// remain in the shared desktop FileTransferController and FileReceiver.
export class FileHost {
  private static grants: preferences.Preferences;
  private static sequence: number = 0;
  static async prepare(context: common.UIAbilityContext): Promise<void> {
    FileHost.grants = await preferences.getPreferences(context, { name: 'slint-file-grants' });
    const folders = await FileHost.grants.get('folders', []) as string[];
    for (const uri of folders) {
      try { await fileShare.activatePermission([FileHost.policy(uri)]); }
      catch (error) { console.warn(`Slint folder permission is no longer available: ${error.code}`); }
    }
    // These are snapshots from previous processes, never live transfers.
    const staging = `${context.cacheDir}/slint-outgoing`;
    if (fileIo.accessSync(staging)) await fileIo.rmdir(staging);
    await fileIo.mkdir(staging);
  }
  private static policy(uri: string): fileShare.PolicyInfo {
    return { uri, operationMode: fileShare.OperationMode.READ_MODE | fileShare.OperationMode.WRITE_MODE };
  }
  static async select(context: common.UIAbilityContext, folder: boolean, owner: window.Window): Promise<string> {
    const selected = await new picker.DocumentViewPicker(context, owner).select({ maxSelectNumber: 1,
      selectMode: folder ? picker.DocumentSelectMode.FOLDER : picker.DocumentSelectMode.FILE });
    if (selected.length === 0) return '';
    const uri = new fileUri.FileUri(selected[0]);
    if (folder) {
      const policy = FileHost.policy(selected[0]);
      await fileShare.persistPermission([policy]);
      await fileShare.activatePermission([policy]);
      const folders = await FileHost.grants.get('folders', []) as string[];
      if (!folders.includes(selected[0])) folders.push(selected[0]);
      await FileHost.grants.put('folders', folders);
      await FileHost.grants.flush();
      return uri.path;
    }
    if (!uri.name || uri.name === '.' || uri.name === '..' || uri.name.includes('/'))
      throw new Error('Invalid selected filename');
    const directory = `${context.cacheDir}/slint-outgoing/${Date.now()}-${++FileHost.sequence}`;
    await fileIo.mkdir(directory);
    const path = `${directory}/${uri.name}`;
    const source = await fileIo.open(selected[0], fileIo.OpenMode.READ_ONLY);
    try { await fileIo.copyFile(source.fd, path); }
    finally { await fileIo.close(source); }
    return path;
  }
}
