# 项目 UI 约定

构建使用 `tools/build.ps1 -VisualStudio 2022 -Configuration Release`。生成文件位于 `build/vs2022`，日常运行和验收入口为 `out/vs2022/Release/DazFastViewer.exe`；验证时确认部署清单与当前源码一致。其他配置和工具链使用各自的 `out/vs年份/配置` 子目录。

修改或新增 Qt UI 时，读取并遵守本工程技能 [dazfastviewer-qt-ui](.agents/skills/dazfastviewer-qt-ui/SKILL.md)。该技能随仓库维护，仅适用于本工程，不安装到用户级技能目录。

用户固定要求：滚轮调参必须先点击选中对应模块／参数行；未选中时滚轮用于外层页面滚动。除非用户明确要求，不嵌套参数滚动区域；表格和列表按内容展开，超出屏幕由最外层滚动条处理。

数值框统一使用地面对齐的原生 Qt 样式；点击选中前后保持上下调节箭头的布局和风格，不用选中态边框样式将其变为两个独立按钮。选中提示放在参数行标签。
