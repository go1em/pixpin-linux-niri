#!/usr/bin/env bash
# PixPin Linux - Release Build Script
set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
RELEASES_DIR="$REPO_ROOT/releases"
VERSION="1.0.8"

echo "=== 正在编译与打包 PixPin Linux v$VERSION ==="

mkdir -p "$BUILD_DIR" "$RELEASES_DIR"

cd "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ..
cmake --build . -j"$(nproc)"
strip pixpin

# CPack DEB
/home/realsky/.local/bin/cpack -G "DEB" 2>/dev/null || cpack -G "DEB"
cp -f "pixpin-${VERSION}-Linux.deb" "$RELEASES_DIR/pixpin_${VERSION}_amd64.deb"

# Universal Portable Tarball
PORTABLE_DIR="/tmp/pixpin-portable/pixpin-${VERSION}-linux-x86_64"
rm -rf /tmp/pixpin-portable
mkdir -p "$PORTABLE_DIR/bin" \
         "$PORTABLE_DIR/share/applications" \
         "$PORTABLE_DIR/share/icons/hicolor/scalable/apps" \
         "$PORTABLE_DIR/share/pixpin"

cp "$BUILD_DIR/pixpin" "$PORTABLE_DIR/bin/"
cp "$REPO_ROOT/pixpin.desktop" "$PORTABLE_DIR/share/applications/"
cp "$REPO_ROOT/pixpin.svg" "$PORTABLE_DIR/share/icons/hicolor/scalable/apps/"
cp "$REPO_ROOT/src/ocr/ocr_worker.py" "$PORTABLE_DIR/share/pixpin/"
cp -rf "$REPO_ROOT/resources/tessdata" "$PORTABLE_DIR/share/pixpin/"
cp "$REPO_ROOT/packaging/portable/install.sh" "$PORTABLE_DIR/"
cp "$REPO_ROOT/packaging/portable/uninstall.sh" "$PORTABLE_DIR/"
cp "$REPO_ROOT/README.md" "$PORTABLE_DIR/"
cp "$REPO_ROOT/LICENSE" "$PORTABLE_DIR/"

tar -czf "$RELEASES_DIR/pixpin-${VERSION}-linux-x86_64.tar.gz" -C /tmp/pixpin-portable "pixpin-${VERSION}-linux-x86_64"
rm -rf /tmp/pixpin-portable

cd "$RELEASES_DIR"
sha256sum pixpin*.deb pixpin*.tar.gz > SHA256SUMS.txt

echo "=== 打包完成！发布文件位于 $RELEASES_DIR ==="
ls -lh "$RELEASES_DIR"
