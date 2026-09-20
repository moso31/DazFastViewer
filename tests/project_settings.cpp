#include "editor/project.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char *why) {if(!ok) throw std::runtime_error(why);}
int main(int argc,char **argv) {
  QCoreApplication app(argc,argv);
  try {
    QTemporaryDir temp;require(temp.isValid(),"临时目录失败");
    const auto a=temp.path()+QStringLiteral("/中文内容库"),b=temp.path()+"/second",file=temp.path()+QStringLiteral("/项目.json");
    dfv::editor::ProjectSettings settings{file,{a,b,a.toUpper(),QDir::toNativeSeparators(b)}};settings.save();
    auto loaded=dfv::editor::ProjectSettings::load(file);require(loaded.content_roots==QStringList{a,b},"根目录顺序 / Unicode / 去重失效");
    loaded.content_roots={b,a};loaded.save();require(dfv::editor::ProjectSettings::load(file).content_roots==QStringList{b,a},"重新打开丢失优先级");
    bool rejected=false;try {loaded.content_roots={"relative/path"};loaded.save();} catch(...) {rejected=true;}
    require(rejected && dfv::editor::ProjectSettings::load(file).content_roots==QStringList{b,a},"无效写入损坏已有配置");
    QFile corrupt(file);require(corrupt.open(QIODevice::WriteOnly),"无法写入损坏用例");corrupt.write("{");corrupt.close();
    rejected=false;try {dfv::editor::ProjectSettings::load(file);} catch(...) {rejected=true;}require(rejected,"损坏配置被静默覆盖");
    std::cout<<"Project settings persistence / priority / Unicode / deduplication / invalid input: PASS\n";
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
