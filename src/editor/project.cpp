#include "editor/project.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSaveFile>
#include <QVBoxLayout>
#include <stdexcept>

namespace dfv::editor {
static void fail(const QString &message) {throw std::runtime_error(message.toUtf8().toStdString());}
QStringList ProjectSettings::normalize(const QStringList &roots) {
  QStringList result;
  for(const auto &root:roots) {
    const auto path=QDir::cleanPath(QDir::fromNativeSeparators(root.trimmed()));
    if(root.trimmed().isEmpty() || !QDir::isAbsolutePath(path)) fail(QStringLiteral("内容库必须使用绝对目录：")+root);
    if(!result.contains(path,Qt::CaseInsensitive)) result.append(path);
  }
  return result;
}
ProjectSettings ProjectSettings::load(const QString &path) {
  ProjectSettings settings;settings.file=QFileInfo(path).absoluteFilePath();QFile file(settings.file);
  if(!file.exists()) return settings;
  if(!file.open(QIODevice::ReadOnly)) fail(QStringLiteral("无法读取项目设置：")+file.errorString());
  QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error);
  if(error.error!=QJsonParseError::NoError || !doc.isObject()) fail(QStringLiteral("项目设置 JSON 无效：")+error.errorString());
  const auto object=doc.object();
  if(object.value("version").toInt()!=1 || !object.value("content_roots").isArray()) fail(QStringLiteral("不支持的项目设置格式"));
  for(const auto &root:object.value("content_roots").toArray()) {
    if(!root.isString()) fail(QStringLiteral("内容库路径必须是字符串"));settings.content_roots.append(root.toString());
  }
  settings.content_roots=normalize(settings.content_roots);return settings;
}
void ProjectSettings::save() const {
  QJsonArray paths;for(const auto &root:normalize(content_roots)) paths.append(root);
  QSaveFile output(file);if(!output.open(QIODevice::WriteOnly)) fail(QStringLiteral("无法保存项目设置：")+output.errorString());
  const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"content_roots",paths}}).toJson(QJsonDocument::Indented);
  if(output.write(bytes)!=bytes.size() || !output.commit()) fail(QStringLiteral("项目设置保存失败：")+output.errorString());
}
bool edit_project_settings(QWidget *parent,ProjectSettings &settings) {
  QDialog dialog(parent);dialog.setWindowTitle(QStringLiteral("项目设置 · DAZ 内容库"));dialog.resize(720,430);
  auto *layout=new QVBoxLayout(&dialog);auto *intro=new QLabel(QStringLiteral("按从上到下的顺序查找资源，同一路径优先使用靠前的库。\n保存后会重新扫描当前场景，并按参数 ID 保留仍然兼容的编辑值。"));intro->setWordWrap(true);layout->addWidget(intro);
  auto *list=new QListWidget;list->addItems(settings.content_roots);layout->addWidget(list,1);
  auto *buttons=new QHBoxLayout;layout->addLayout(buttons);
  auto button=[&](const QString &name,auto callback) {auto *b=new QPushButton(name);buttons->addWidget(b);QObject::connect(b,&QPushButton::clicked,&dialog,callback);};
  button(QStringLiteral("添加目录…"),[&] {const auto path=QFileDialog::getExistingDirectory(&dialog,QStringLiteral("选择包含 data / Runtime 的内容库根目录"));if(!path.isEmpty()) {QStringList paths;for(int i=0;i<list->count();++i) paths.append(list->item(i)->text());paths.append(path);list->clear();list->addItems(ProjectSettings::normalize(paths));}});
  button(QStringLiteral("移除"),[&] {delete list->takeItem(list->currentRow());});
  auto move=[&](int delta) {const int row=list->currentRow(),next=row+delta;if(row>=0 && next>=0 && next<list->count()) {auto *item=list->takeItem(row);list->insertItem(next,item);list->setCurrentRow(next);}};
  button(QStringLiteral("上移"),[&] {move(-1);});button(QStringLiteral("下移"),[&] {move(1);});buttons->addStretch();
  auto *location=new QLabel(QStringLiteral("保存文件：")+QDir::toNativeSeparators(settings.file));location->setWordWrap(true);location->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(location);
  auto *status=new QLabel;status->setWordWrap(true);layout->addWidget(status);
  auto *box=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);box->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存并应用"));layout->addWidget(box);
  QObject::connect(box,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  QObject::connect(box,&QDialogButtonBox::accepted,&dialog,[&] {
    try {auto updated=settings;updated.content_roots.clear();for(int i=0;i<list->count();++i) updated.content_roots.append(list->item(i)->text());updated.save();settings=std::move(updated);dialog.accept();}
    catch(const std::exception &e) {status->setText(QString::fromUtf8(e.what()));}
  });
  for(int i=0;i<list->count();++i) if(!QFileInfo(list->item(i)->text()).isDir()) {list->item(i)->setToolTip(QStringLiteral("目录当前不可访问，配置会保留"));list->item(i)->setForeground(Qt::darkRed);}
  return dialog.exec()==QDialog::Accepted;
}
}
