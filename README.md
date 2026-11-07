# PixPin Linux 📌

一款专为 Linux (Ubuntu / Debian / Arch / Fedora 等) 深度定制的原生 **PixPin** 复刻版。采用 **C++17 + Qt 6 + Tesseract / PaddleOCR (ONNX)** 原生构建，具备极致轻量、零延迟启动、双屏/高分屏平滑渲染、深度适配 **Ubuntu Wayland** 与 X11 显示架构等特点。

![Qt6](https://img.shields.io/badge/Qt-6.4+-41CD52?style=flat&logo=qt)
![C++17](https://img.shields.io/badge/C++-17-00599C?style=flat&logo=c%2B%2B)
![Wayland](https://img.shields.io/badge/Display-Wayland%20%7C%20X11-orange)
![OCR](https://img.shields.io/badge/OCR-PaddleOCR%20(RapidOCR)%20%2B%20Tesseract-blue)
![License](https://img.shields.io/badge/License-MIT-green.svg)

---

## 🌟 核心特性

| 功能模块 | 特性说明 |
| :--- | :--- |
| **Wayland / X11 双架构** | 完美适配 GNOME/KDE Wayland（`XDG Desktop Portal`）及传统 X11；在 niri / Sway / Hyprland 等合成器上，若系统没有 Portal 截图后端，会自动回退到 `grim`（wlr-screencopy），不再静默失败。 |
| **极致 4K 160Hz 性能** | 采用局部脏区刷新（Dirty-rect Damage Blit）与打孔透视算法，拖拽帧时间仅 **0.09ms (10,842 FPS)**，4K 极速丝滑无拖影。 |
| **矩形 / 圆形 / 椭圆截图** | 工具栏一键切换选区形状（或快捷键 `Tab` / `R` / `E`），按住 `Shift` 拖拽自动锁定 **1:1 正圆 / 正方形**。 |
| **透明背景 PNG 导出** | 圆形与椭圆截图导出时外部区域保留 **100% 真实 Alpha 透明**，保存 PNG 或复制剪贴板时完美无白边、无黑角。 |
| **核心贴图 (Pin to Desktop)**| 截选任意图像一键脱离置顶贴图（快捷键 `Ctrl+P`），支持滚轮无级缩放（0.2x~5.0x）、`Ctrl+滚轮` 调节透明度、透明贴图自适应去除边框。 |
| **双引擎高精度离线 OCR** | 集成 **PaddleOCR PP-OCRv4 (RapidOCR ONNX)** + **Tesseract LSTM** 双引擎。精准过滤背景干扰，智能识别艺术字、视频封面与场景文字。 |
| **全功能标注涂鸦** | 矩形框、圆形框、导向箭头、自由画笔、数字序号气球 (1, 2, 3...)、局部马赛克/模糊、富文本输入、动态调色板与撤销重做。 |
| **双屏 / 异构多显示器** | 完整支持多屏幕拼接几何布局与混合 DPI 缩放，屏幕间平滑移动无缝接壤。 |

---

## 📦 下载与安装

已提供主流 Linux 发行版的预编译安装包（位于 [Releases 页面](https://github.com/realsky-lgh/pixpin-linux/releases) 或本地 `releases/` 目录）：

### 1. Ubuntu / Debian / Linux Mint / Deepin / UOS (`.deb`)

```bash
# 下载并安装 deb 软件包
sudo dpkg -i pixpin_1.0.8_amd64.deb

# 如果缺少基础依赖，执行自动修复
sudo apt-get install -f -y
```

### 2. 通用免编译便携版 (Universal Linux x86_64 `.tar.gz`)

适用于任何 Linux 发行版（Fedora、Arch、openSUSE、CentOS 等）：

```bash
tar -xzvf pixpin-1.0.8-linux-x86_64.tar.gz
cd pixpin-1.0.8-linux-x86_64

# 免 root 安装到 ~/.local (推荐)：
./install.sh

# 或系统级安装到 /usr/local：
sudo ./install.sh
```

如需卸载，运行 `./uninstall.sh` 即可彻底清除。

### 3. Arch Linux / Manjaro

项目内置 PKGBUILD，源码直接取自仓库本身（无需预打包 tarball）：

```bash
git clone https://github.com/go1em/pixpin-linux-niri.git
cd pixpin-linux-niri/packaging/arch
makepkg -si
```

### 4. niri / Sway / Hyprland 等合成器

这类合成器默认没有可用的 `org.freedesktop.portal.Screenshot` 后端（Arch 上只装 `xdg-desktop-portal-gtk` 时它并不实现该接口）。PixPin 在启动时探测该接口，不可用即回退到 `grim`，无需额外配置：

```bash
sudo pacman -S grim          # 回退方案（wlr-screencopy）
# 或：sudo pacman -S xdg-desktop-portal-gnome   # 提供 Portal 截图后端
```

PixPin 没有内置全局热键设置，快捷键由合成器/桌面环境绑定到命令行。以 niri 为例（`~/.config/niri/config.kdl`）：

```kdl
binds {
    Ctrl+F1 { spawn "pixpin" "--capture"; }
    Ctrl+F2 { spawn "pixpin" "--pin"; }
}
```

---

## ⚡ 启用高精度 PaddleOCR (RapidOCR)

PixPin 默认自带系统级 Tesseract OCR 引擎。若需识别复杂场景文字、视频封面标题或极高准确率的汉字识别，可一键安装 RapidOCR 支持库：

```bash
# 运行内置的一键配置脚本 (免 sudo，自动创建独立隔离环境)：
./scripts/setup-ocr.sh
```

该脚本将会在 `~/.local/share/pixpin/venv` 中自动安装 ONNX 运行环境与 PaddleOCR PP-OCRv4 轻量级离线模型，PixPin 启动时将自动无缝接管 OCR 识别。

---

## ⌨️ 快捷键与操作指南

### 截屏与交互

| 快捷键 / 操作 | 动作功能 |
| :--- | :--- |
| `Alt + 1` / `Ctrl + 1` | 唤起截屏（系统自定义全局快捷键绑定 `pixpin -c`） |
| `鼠标左键拖拽` | 框选截屏区域 |
| `Shift + 拖拽` | **等比例锁定**：椭圆模式锁定为**正圆**，矩形模式锁定为**正方形** |
| `Tab` / `鼠标滚轮` | 切换选区形状（矩形 ↔ 椭圆/圆形） |
| `R` | 直接切换为矩形截屏模式 |
| `E` | 直接切换为椭圆/圆形截屏模式 |
| `Ctrl + P` | 将当前截图**钉在桌面上（贴图）** |
| `Ctrl + C` / 双击选区 | 复制当前截图到剪贴板并退出 |
| `Shift + C` | **提取文字并自动复制到剪贴板**（支持繁体中文、简体中文及英文高精度 OCR） |
| `Ctrl + S` | 保存截图为文件（支持透明 PNG、JPG、BMP） |
| `Ctrl + Z` | 撤销上一步涂鸦/标注操作 |
| `Esc` / 鼠标右键 | 取消并退出截屏 |

### 桌面贴图操作 (Pin Window)

| 操作 | 功能说明 |
| :--- | :--- |
| `鼠标左键拖拽` | 移动贴图窗口位置 |
| `Shift + C` | **提取贴图文字并自动复制到剪贴板** |
| `Ctrl + C` | 复制贴图图片到剪贴板 |
| `Ctrl + S` | 保存贴图为图片文件 |
| `鼠标滚轮` | 缩放贴图大小 (0.2x ~ 5.0x 无级缩放) |
| `Ctrl + 鼠标滚轮` | 调节贴图透明度 (20% ~ 100%) |
| `鼠标双击` / `Esc` | 关闭贴图 |
| `鼠标右键` | 打开上下文菜单：复制、另存为、文字识别提取、水平/垂直镜像翻转、置顶锁定 |

---

## ⚙️ 系统全局快捷键绑定 (Ubuntu GNOME)

在 Ubuntu GNOME 桌面环境下，推荐将全局截图快捷键绑定到 `pixpin -c`：

1. 打开系统 **设置 (Settings)** -> **键盘 (Keyboard)** -> **键盘快捷键 (Keyboard Shortcuts)** -> **查看及自定义快捷键 (View and Customize Shortcuts)**。
2. 滑动到最底部，点击 **自定义快捷键 (Custom Shortcuts)** -> 点击 **+** 号：
   - **名称**：`PixPin 截屏`
   - **命令**：`pixpin -c`
   - **快捷键**：按下您习惯的组合键（例如 `Alt + 1` 或 `Ctrl + Alt + A`）
3. 绑定贴图快捷键（可选）：
   - **名称**：`PixPin 贴图`
   - **命令**：`pixpin -p`
   - **快捷键**：按下 `Alt + 2` 或 `Ctrl + Alt + P`

---

## 🛠️ 从源码构建

如果您希望自行从源码编译 PixPin Linux：

### 1. 安装构建依赖

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install -y build-essential cmake ninja-build \
    qt6-base-dev libqt6svg6-dev \
    libtesseract-dev libleptonica-dev \
    tesseract-ocr-chi-sim tesseract-ocr-eng

# Fedora
sudo dnf install -y gcc-c++ cmake ninja-build \
    qt6-qtbase-devel qt6-qtsvg-devel \
    tesseract-devel leptonica-devel

# Arch Linux
sudo pacman -S --needed base-devel cmake ninja \
    qt6-base qt6-svg tesseract leptonica
# 可选：无 Portal 截图后端时用于回退
sudo pacman -S grim
```

### 2. 编译与打包

```bash
git clone https://github.com/go1em/pixpin-linux-niri.git
cd pixpin-linux-niri

# 一键编译并打包 Release 版本
./scripts/build-release.sh
```

编译生成的可执行文件位于 `build/pixpin`，安装包位于 `releases/`。

---

## 📁 目录结构

```
pixpin-linux/
├── CMakeLists.txt              # CMake 构建与 CPack 打包配置
├── LICENSE                     # MIT 开源许可证
├── README.md                   # 详细使用与开发说明
├── INSTALL.md                  # 各 Linux 发行版安装指引
├── pixpin.desktop              # XDG 标准桌面快捷入口
├── pixpin.svg                  # 高清矢量应用图标
├── packaging/                  # 针对各发行版的打包元数据
│   ├── arch/PKGBUILD           # Arch Linux 打包配方
│   ├── debian/                 # Debian/Ubuntu postinst/postrm 脚本
│   ├── portable/               # 通用便携安装/卸载脚本 (install.sh / uninstall.sh)
│   └── rpm/pixpin.spec         # Fedora/RPM 打包规范
├── releases/                   # 预编译二进制发行文件
│   ├── pixpin_1.0.8_amd64.deb
│   ├── pixpin-1.0.8-linux-x86_64.tar.gz
│   └── SHA256SUMS.txt
├── scripts/                    # 实用运维与打包脚本
│   ├── build-release.sh        # 一键 Release 构建脚本
│   └── setup-ocr.sh            # RapidOCR 离线环境一键配置脚本
└── src/                        # 核心 C++ 源代码
    ├── app/                    # 应用程序核心与系统托盘管理
    ├── capture/                # 4K 高分屏截屏遮罩、放大镜、跨屏捕获
    ├── ui/                     # 工具栏、全能涂鸦标注层、置顶贴图窗
    ├── ocr/                    # OCR 引擎管理与暗黑风格提取弹窗
    └── utils/                  # 图像算法、几何计算与透明蒙版处理
```

---

## 📄 开源许可证

本项目基于 [MIT 许可证](LICENSE) 开源。欢迎提交 Issue 与 Pull Request！
