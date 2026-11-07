# PixPin Linux 安装指南 (Installation Guide)

本文档详细说明如何在各主流 Linux 操作系统上安装、配置与使用 PixPin Linux。

---

## 目录
1. [系统要求](#系统要求)
2. [Ubuntu / Debian / Linux Mint / Deepin / UOS](#1-ubuntu--debian--linux-mint--deepin--uos)
3. [通用免编译便携版 (任何 Linux 发行版)](#2-通用免编译便携版-任何-linux-发行版)
4. [Arch Linux / Manjaro](#3-arch-linux--manjaro)
5. [Fedora / RHEL / CentOS Stream](#4-fedora--rhel--centos-stream)
6. [启用高精度 PaddleOCR (RapidOCR)](#5-启用高精度-paddleocr-rapidocr)
7. [Wayland 与全局快捷键配置](#6-wayland-与全局快捷键配置)
8. [常见问题与排错 (FAQ)](#7-常见问题与排错-faq)

---

## 系统要求

- **操作系统**: Linux (内核 5.4 及以上)
- **桌面环境**: GNOME、KDE Plasma、XFCE、DDE、Cinnamon、MATE 等
- **显示服务**: Wayland (GNOME/KDE/Portal 兼容) 或 X11
- **Wayland 截图后端**: `xdg-desktop-portal` + 实现 `Screenshot` 的后端（如 `xdg-desktop-portal-gnome`），或 `grim`（niri/Sway/Hyprland 场景自动回退）
- **硬件架构**: x86_64 / amd64
- **基础依赖**: Qt 6.4+, Tesseract 5.x, Leptonica

---

## 1. Ubuntu / Debian / Linux Mint / Deepin / UOS

我们提供了标准的 `.deb` 预编译包。

### 安装步骤

```bash
# 1. 下载 deb 包 (从 Releases 或仓库 releases/ 目录)
cd releases/

# 2. 使用 dpkg 进行安装
sudo dpkg -i pixpin_1.0.8_amd64.deb

# 3. 自动安装缺失的系统库依赖
sudo apt-get install -f -y
```

### 卸载

```bash
sudo apt remove pixpin
```

---

## 2. 通用免编译便携版 (任何 Linux 发行版)

适用于任何主流 64 位 Linux 系统。无需 root 权限即可安装在用户目录中。

### 安装步骤

```bash
# 1. 解压便携包
tar -xzvf pixpin-1.0.8-linux-x86_64.tar.gz
cd pixpin-1.0.8-linux-x86_64

# 2. 普通用户安装 (安装到 ~/.local，推荐)
./install.sh

# 或 系统级安装 (需要 root 权限，安装到 /usr/local)
sudo ./install.sh
```

### 卸载

```bash
./uninstall.sh
# 或若是 sudo 安装：
sudo ./uninstall.sh
```

---

## 3. Arch Linux / Manjaro

Arch 用户可直接使用项目内置的 PKGBUILD（源码取自仓库本身，不需要额外下载 tarball）：

```bash
git clone https://github.com/go1em/pixpin-linux-niri.git
cd pixpin-linux-niri/packaging/arch
makepkg -si
```

### niri / Sway / Hyprland

这些合成器默认没有实现 `org.freedesktop.portal.Screenshot` 的后端（Arch 上通常只装了 `xdg-desktop-portal-gtk`，它不实现该接口）。PixPin 会在启动时探测该接口，缺失时自动回退到 `grim`：

```bash
sudo pacman -S grim                      # 回退方案，wlr-screencopy
# 二选一：sudo pacman -S xdg-desktop-portal-gnome   # 提供 Portal 截图后端
```

安装后需在合成器里绑定快捷键，例如 niri（`~/.config/niri/config.kdl`）：

```kdl
binds {
    Ctrl+F1 { spawn "pixpin" "--capture"; }
    Ctrl+F2 { spawn "pixpin" "--pin"; }
}
```

改完执行 `niri msg action load-config-file` 生效。

---

## 4. Fedora / RHEL / CentOS Stream

### 安装编译依赖与构建

```bash
sudo dnf install -y gcc-c++ cmake ninja-build \
    qt6-qtbase-devel qt6-qtsvg-devel \
    tesseract-devel leptonica-devel

git clone https://github.com/realsky/pixpin-linux.git
cd pixpin-linux
./scripts/build-release.sh
```

---

## 5. 启用高精度 PaddleOCR (RapidOCR)

PixPin 采用双引擎设计：
- **默认引擎**: Tesseract OCR（系统原生 C++ 绑定）
- **高阶引擎**: PaddleOCR PP-OCRv4 (基于 ONNXRuntime 深度学习推理)

若需在视频截屏、低对比度海报、花体字中获得近乎 100% 的识别准确率，请执行以下命令配置 RapidOCR 隔离环境：

```bash
./scripts/setup-ocr.sh
```

该脚本将会在 `~/.local/share/pixpin/venv` 中自动安装 ONNXRuntime 与轻量化 PaddleOCR 模型，无需污染全局 Python 环境。

---

## 6. Wayland 与全局快捷键配置

### Ubuntu GNOME 桌面设置
1. 打开系统 **设置 (Settings)** -> **键盘 (Keyboard)** -> **键盘快捷键 (Keyboard Shortcuts)** -> **查看及自定义快捷键**。
2. 点击最下方的 **+** (添加自定义快捷键)：
   - **名称**: `PixPin 截图`
   - **命令**: `pixpin -c`
   - **快捷键**: 按下 `Alt + 1` (或 `Ctrl + Alt + A`)
3. 再添加贴图快捷键：
   - **名称**: `PixPin 贴图`
   - **命令**: `pixpin -p`
   - **快捷键**: 按下 `Alt + 2`

### niri / wlroots 系合成器

PixPin 本身不提供快捷键设置界面，常驻进程通过 unix socket 接收命令，快捷键由合成器绑定到 `pixpin --capture` / `pixpin --pin` 即可（冷启动也只有一次开销，之后是毫秒级唤醒）。

```kdl
// ~/.config/niri/config.kdl
binds {
    Ctrl+F1 { spawn "pixpin" "--capture"; }
    Ctrl+F2 { spawn "pixpin" "--pin"; }
}
```

### KDE Plasma 桌面设置
1. 打开 **系统设置** -> **快捷键** -> **自定义快捷键**。
2. 添加新的快捷键命令：
   - 操作: `pixpin -c`
   - 触发按键: 自定义按键

---

## 7. 常见问题与排错 (FAQ)

### Q: Wayland 下提示权限拒绝或用户取消截屏？
**A**: 在某些 GNOME Wayland 环境下，首次截屏系统安全中心会弹出 "允许共享屏幕" 对话框，请点击 "允许" 并勾选记住授权。

### Q: 按了快捷键完全没反应（niri / Sway / Hyprland）？
**A**: 说明当前的 Wayland 截图通路全断了。PixPin 优先走 `org.freedesktop.portal.Screenshot`，没有该接口时回退 `grim`。先手动验证：

```bash
gdbus introspect --session --dest org.freedesktop.portal.Desktop \
    --object-path /org/freedesktop/portal/desktop | grep Screenshot
grim /tmp/t.png && echo grim-ok
```

`Screenshot` 无输出且 `grim` 报错，就装 `grim`（或 `xdg-desktop-portal-gnome`）。两者都装后仍无反应，用 `pixpin --capture` 手动跑一次看终端报错。

### Q: 4K 屏幕上是否支持高刷？
**A**: PixPin Linux 专门针对 4K 160Hz / 144Hz 电竞屏做了局部脏区刷新优化，截取和拖拽过程中帧延迟低于 0.1ms，极其流畅。

### Q: 如何保存透明背景的圆形/椭圆截图？
**A**: 按 `Tab` 键切换至圆形/椭圆模式，框选截屏区域（按住 `Shift` 拖拽可锁定为完美正圆）。点击保存为 `.png` 或按 `Ctrl+C` 复制，外部像素会自动填充为 100% 透明 Alpha 通道。
