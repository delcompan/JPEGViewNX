# JPEGViewNX

用 C++ / Win32 写的极简看图软件。

## 功能

- **透明背景**：棋盘格 / 纯色 / 窗口底色，实时切换；棋盘格不随缩放变形、不随平移抖动
- **格式**：JPEG、PNG、BMP、GIF（含动画）、TIFF、WebP、TGA、JPEG XR、JPEG XL、
  HEIC/AVIF、DDS、ICO/CUR、PCX、PNM，以及相机 RAW 内嵌 JPEG（CR2/CR3/NEF/ARW/DNG/RW2/RAF…）
- **查看**：适应窗口、100%、滚轮缩放、拖动平移、旋转、镜像、全屏、文件夹内翻页
- **文件关联**：弹窗勾选格式，注册进 Windows “默认应用”
- **界面**：中文 / English

## 构建

需要 Visual Studio 2019/2022（MSVC x64）与 Windows 10 SDK。

```
build.bat
```

构建时会自动调用 `tools/make_icon.ps1`，从 `icon/` 里的图片生成 `res/app.ico`
（10 种尺寸），再编译链接出 `bin/JPEGViewNX.exe`。

## 运行

```
bin\JPEGViewNX.exe                 :: 打开窗口，拖入图片或用 Ctrl+O
bin\JPEGViewNX.exe 图片路径
bin\JPEGViewNX.exe --associate-all      :: 关联全部格式（HKCU，无需管理员）
bin\JPEGViewNX.exe --unassociate-all    :: 取消全部关联
```

常用快捷键：`+/-` 缩放、`Space` 适应/100%、`C` 切换透明背景、`P` 暂停动画、
`F11` 全屏、`R` 旋转、`I` 文件名、`T` 状态行。右键菜单有完整命令和文件关联窗口。

## 说明

- 文件关联写入 HKCU，不需要管理员权限。Windows 10/11 不允许程序静默修改默认程序，
  首次设为默认需要在系统设置里确认一次；此后双击图片即可直接打开。
- RAW 默认优先读取内嵌 JPEG 预览（快）；安装 Microsoft Raw Image Extension 后
  可解码全尺寸 RAW（`RawMode=full`）。
- 参考项目：https://github.com/sylikc/jpegview
