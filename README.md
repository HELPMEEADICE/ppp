# Poppin’Party Terminal Logo

在终端中以彩色字符绘制 Poppin’Party 标志。程序使用内嵌的高分辨率像素数据，不需要运行时读取 SVG 或安装第三方库。

## 功能

- 根据终端宽高自动选择合适的绘制尺寸，并尽量使用可用空间。
- 支持 ANSI 24 位真彩色；通过 Unicode 上半块、下半块和全块字符绘制细致图像。
- 为不支持彩色控制序列的输出提供字符密度渲染。
- 针对高分辨率输出使用逐行缓冲和颜色状态缓存，减少终端输出开销。
- Windows 下启用并在退出时恢复终端的 VT 和 UTF-8 设置。

推荐使用支持 UTF-8、ANSI/VT 控制序列和半块字符的等宽字体。Windows 可使用 Windows Terminal；其他平台可使用常见的现代终端模拟器。字符字形和终端渲染方式会影响最终观感。

## 使用

从 GitHub Releases 下载与你的操作系统和 CPU 架构对应的可执行文件。Linux/macOS/BSD 下载后可在终端运行：

```sh
chmod +x poppin_party-linux-x86_64
./poppin_party-linux-x86_64
```

Release 中的 Linux/macOS 文件名包含目标平台，例如 `poppin_party-linux-aarch64`、`poppin_party-macos-arm64`。Windows 文件为 `.exe`，可在 PowerShell 或 Windows Terminal 中运行。

程序只在标准输出连接到交互式终端且终端支持颜色时启用彩色模式。设置 `NO_COLOR`，或将输出重定向到文件/管道时，会使用纯文本渲染。例如：

```sh
NO_COLOR=1 ./poppin_party-linux-x86_64
```

## 从源码构建

需要 C11 兼容的 GCC 或 Clang。Logo 像素数据已包含在 `poppin_party_logo.h` 中。

Windows（MinGW-w64）：

```powershell
gcc -std=c11 -O2 -Wall -Wextra -pedantic .\poppin_party.c -o .\poppin_party.exe
```

Linux：

```sh
cc -std=c11 -O2 -Wall -Wextra -pedantic poppin_party.c -o poppin_party
```

macOS：

```sh
clang -std=c11 -O2 -Wall -Wextra -pedantic poppin_party.c -o poppin_party
```

## Release 构建目标

每次发布会构建 20 个平台/架构目标。

在 GitHub 上发布一个 Release（发布已有草稿也可以）后，`.github/workflows/release.yml` 会自动编译并把二进制和 `SHA256SUMS.txt` 上传为该 Release 的附件。也可以从 Actions 页面手动运行构建矩阵来验证编译；手动运行不会上传 Release。BSD 目标通过 GitHub Actions 中的虚拟机编译：

| 平台 | 架构 |
| --- | --- |
| Windows | x86_64、i686 |
| Linux | x86_64（glibc、musl）、i686、armel、armv7 hard-float、aarch64、riscv64、ppc64le、s390x |
| macOS | x86_64、arm64 / Apple Silicon |
| BSD 系 | FreeBSD x86_64/arm64、OpenBSD x86_64/arm64、NetBSD x86_64/arm64、DragonFly BSD x86_64 |

Linux 版本使用 Ubuntu 22.04 工具链构建；交叉工具链会显式安装目标架构的 libc 开发文件。glibc 版本跨发行版运行时仍需满足目标架构对应的 libc 兼容要求。BSD 版本由对应系统的虚拟机原生编译器构建。跨架构的可执行文件需要在相应硬件或模拟环境中运行。

## 许可证与图案素材

本仓库中的程序代码依照 [GNU General Public License v3.0](LICENSE) 发布。

`poppin_party_logo.h` 包含从用户提供的 Poppin’Party 标志 SVG 生成的像素数据。该标志图案及相关商标、著作权归其各自权利人所有；仓库中的 GPL 代码许可不改变图案素材本身的权利归属。
