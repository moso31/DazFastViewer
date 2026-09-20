# Qt 使用记录

- 本机套件：`C:\Qt\6.10.3\msvc2022_64`，`qmake -query QT_VERSION` = `6.10.3`，共享库 MSVC x64。
- 直接链接：Qt Core、Gui、Widgets；由官方 windeployqt 暂存 Windows 平台、图片、样式和中文翻译插件，附带 Qt Network / Svg 依赖。
- 只动态链接以上 LGPL 可用模块；Qt 不进入纯 C++ Runtime 或 DAZ 数据模型。
- 上游：[Qt](https://www.qt.io/)、[许可证说明](https://doc.qt.io/qt-6/licensing.html)。
- 源码版本：[qtbase v6.10.3](https://code.qt.io/cgit/qt/qtbase.git/tree/?h=v6.10.3)、[官方源码归档](https://download.qt.io/official_releases/qt/6.10/6.10.3/submodules/)。
- 本目录保留上游 LGPL-3.0 与 GPL-3.0 正文；来源是 qtbase 对应标签的 `LICENSES`。依赖构件与附加第三方许可证清单来自已安装套件 `sbom`，暂存到 `out/licenses/qt`。
- 当前为本地开发，工程整体发布审计仍未完成。发布前须完成依赖通知、对应源码获取 / 提供及库替换等具体分发要求；不能仅凭动态链接认定全部发布义务完成。
