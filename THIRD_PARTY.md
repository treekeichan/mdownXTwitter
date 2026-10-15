# 第三方组件

本项目通过进程调用外部下载工具，不将其源码编译进桌面应用。

- Qt: https://www.qt.io/licensing/ 。运行时由 windeployqt 从使用者的 Qt 安装复制。具体模块和许可见 Qt 安装目录中的许可材料。
- yt-dlp: https://github.com/yt-dlp/yt-dlp 。源码的 Unlicense 与 Windows 打包二进制中的依赖许可不同，官方 README 明确说明其 PyInstaller 合集包含 GPLv3+ 代码。
- FFmpeg: https://ffmpeg.org/legal.html 。准备脚本下载 yt-dlp/FFmpeg-Builds 的 Windows GPL 构建：https://github.com/yt-dlp/FFmpeg-Builds 。实际许可与编译选项有关。

setup-tools.ps1 保留下载版本记录和 FFmpeg 上游压缩包。dist 为本机运行准备的便携目录，不代表已完成第三方再分发材料审核。
