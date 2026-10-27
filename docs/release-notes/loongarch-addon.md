> **LoongArch 支持（本仓库附加）/ LoongArch add-on（this repository）**

- **LoongArch64 支持**：提供 `loong64`（Debian/Loongnix 架构名）与 `loongarch64`（AOSC 架构名）两种 deb 包，二者为同一 ABI 2.0 二进制。已在 Loongnix 25 / 3A6000 上实测：X11 采集、VAAPI 硬编码（RX 580）、音频采集、文件传输、终端均正常。
  LoongArch64 support with both `loong64` (Debian/Loongnix) and `loongarch64` (AOSC) deb packages — the same ABI 2.0 binary. Tested on Loongnix 25 / 3A6000 (X11 capture, VAAPI encoding, audio, file transfer, terminal).
- **VAAPI 编码器**：色彩空间转换与缩放移到 GPU（`scale_vaapi`），降低编码 CPU 占用。
  VAAPI encoder: colorspace conversion and scaling now run on the GPU.
- Release workflow 通过 QEMU + Debian loong64 容器构建 LoongArch 包，随正式发布一起上传（与 x86_64 / aarch64 相同流程）；升级 vendor 的 webrtc/nix 组件以支持 loongarch64 构建。
  The release workflow builds LoongArch packages under QEMU (Debian loong64 container) and uploads them with every release; vendored webrtc/nix components upgraded for loongarch64 builds.
- 本仓库由自动化定时跟踪上游发布，上游出新版后自动合并、打同名 tag 并重新构建四架构包。
  This repository tracks upstream releases automatically: when a new upstream release appears, it is merged, tagged with the same tag name, and all four architectures are rebuilt.
