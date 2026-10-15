# mdownXTwitter

Windows 桌面工具，使用 C++17 / Qt 6 Widgets，通过独立的 yt-dlp 进程解析和下载视频。

## 编译

需要 Windows、Qt 6.5 或更高版本（Widgets 和 Test 模块）、匹配的 MinGW 编译器、CMake 3.21 或更高版本及 Ninja。脚本默认使用 `C:\Qt` 下的常见安装路径，可按实际安装位置调整。

```powershell
.\scripts\build.ps1
.\scripts\setup-tools.ps1 -IncludeFFmpeg
.\scripts\build.ps1 -Package
.\dist\mdownXTwitter.exe
```

`build.ps1` 默认编译并运行测试。可传 `-QtRoot`、`-CompilerRoot` 使用其他 Qt/MinGW 路径。依赖准备需要访问 GitHub，脚本从上游 release 下载并校验 SHA256；不修改系统 PATH 或安装全局 Python 包。若已有依赖，可跳过准备脚本，在界面中指定可执行文件和 FFmpeg 目录。

开发时在 Qt Creator 打开 `CMakeLists.txt`，选择对应的 **Qt 6 MinGW 64-bit** Kit。编译产物为 `build/mdownXTwitter.exe`，便携版入口为 `dist/mdownXTwitter.exe`。准备依赖后应重新打包，以把引擎一并复制到 dist。

## 使用

1. 在“引擎、代理和登录设置”中确认 yt-dlp 路径；FFmpeg 指定同时含 ffmpeg.exe、ffprobe.exe 的目录。
2. 可按需填写 HTTP/HTTPS/SOCKS5 代理，或选择 Netscape 格式 Cookie 文件。Cookie 内容不写入应用日志；程序不保存账号密码。yt-dlp 可能更新所选 Cookie 文件，请使用专门导出的副本。
3. 粘贴完整帖子地址，每行一个，点击“解析并添加”。支持 x.com、twitter.com 域名；相同帖子的跟踪参数会移除。
4. 每个视频选择清晰度，再点击“下载所有就绪任务”。同一帖子中的多个视频会分别列出；队列串行下载，减少触发限流的概率。
   也可先选中任务，再点击单独的“开始下载”，只将选中的就绪任务加入下载队列（支持 Ctrl/Shift 多选）。
5. 选中行后可取消、重试或打开目录。重试下载失败的任务后，需再点击下载按钮。取消会终止引擎及其 FFmpeg 子进程，保留部分下载文件供再次下载续传。
6. “清空记录”会清除已完成、失败和已取消的记录，并同步清理已保存的完成历史；保留待解析、就绪、排队中和正在处理的任务，不删除本地视频文件。

应用只保存最近 200 条已完成记录，位于 Qt `AppLocalDataLocation` 下的 `history.json`。界面配置由 QSettings 存放在当前 Windows 用户配置中；不持久保存代理账号密码。程序重启不会自动恢复未完成任务。

“最佳可用”优先选择自带音频的 MP4，其次其他完整格式，再尝试分离音视频合并。最高 1080p / 720p 是分辨率上限，不进行放大或转码。下载记录命名为作者 ID + 视频 ID，避免同帖多视频互相覆盖。平台没有提供的清晰度不会凭空生成。

## 验证

```powershell
.\scripts\build.ps1
.\dist\mdownXTwitter.exe --smoke-test
```

测试覆盖引擎元数据解析、进度解析、中文保存路径、异常 JSON、限流、启动失败、取消后复用，以及界面中的链接去重、多视频拆分、清晰度保留和顺序下载。常规测试引擎为本地子进程，不访问 X；便携版启动检查生成 `dist/smoke-test.txt` 和界面截图 `dist/smoke-test.png`，使用独立的配置命名空间。

真实网络测试需主动运行（下载到项目的 `build/integration-download`）：

```powershell
.\scripts\test-real-download.ps1 -Url 'https://x.com/username/status/1234567890'
```

请将示例地址替换为自己有权访问的真实视频帖子链接。真实网络测试会产生下载文件；常规自动测试不需要 GitHub 或 X 账号。

实际 X 下载仍依赖网络、上游解析器和帖子访问权限；需要用可访问的真实链接验证。程序没有嵌入浏览器登录、账户管理、自动引擎更新或批量账号抓取功能。

## 代码结构

- `src/mainwindow.*`：桌面界面、顺序队列、格式选择、配置、完成历史。
- `src/downloadengine.*`：QProcess 生命周期、结构化输出解析、错误分类、取消。
- `tests/`：独立假引擎和 Qt Test 自动测试。
- `scripts/build.ps1`：配置、编译、测试和 Qt 运行库部署。
- `scripts/setup-tools.ps1`：下载并校验外部引擎。

打包使用 Qt 动态库；Qt、yt-dlp Windows 二进制及 FFmpeg 各有上游许可，向他人分发前需随包准备对应许可和必要的源代码信息。详见 `THIRD_PARTY.md`。

## 仓库内容与隐私

仓库仅包含源码、测试、构建脚本和文档。可执行文件、第三方二进制、下载视频、截图、日志、Cookie、历史记录以及个人 IDE 配置不纳入版本管理。准备依赖和构建时会在本地创建 `tools/`、`build/`、`dist/` 目录。
