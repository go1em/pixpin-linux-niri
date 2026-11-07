Name:           pixpin
Version:        1.0.8
Release:        1%{?dist}
Summary:        Smart Screenshot, Pin and Offline OCR tool for Linux

License:        MIT
URL:            https://github.com/realsky/pixpin-linux
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  ninja-build
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtsvg-devel
BuildRequires:  tesseract-devel
BuildRequires:  leptonica-devel

Requires:       qt6-qtbase
Requires:       qt6-qtsvg
Requires:       tesseract
Requires:       leptonica
Recommends:     python3
Recommends:     python3-pip

%description
A native Linux implementation of PixPin with 4K 160Hz rendering,
pin-to-desktop, rectangular and circular/elliptical screenshots with
transparent PNG export, and dual OCR engines (PaddleOCR RapidOCR + Tesseract).

%prep
%autosetup

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install

%files
%{_bindir}/pixpin
%{_datadir}/applications/pixpin.desktop
%{_datadir}/icons/hicolor/scalable/apps/pixpin.svg
%{_datadir}/pixpin/ocr_worker.py

%changelog
* Sat Nov 07 2026 realsky <realsky@local> - 1.0.8-1
- Fall back to grim when no portal Screenshot backend is available

* Sat Oct 17 2026 realsky <realsky@local> - 1.0.0-1
- Initial 1.0.0 release
