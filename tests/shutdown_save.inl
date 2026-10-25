static void shutdown_save_checks() {
  QTemporaryDir temp;check(temp.isValid(),"自动保存测试目录不可用");const auto folder=std::filesystem::path(temp.path().toStdWString());
  auto state=disk_fixture(folder);const auto directory=temp.filePath("portable");QDir().mkpath(directory);
  check(shutdown_save_directory(directory,temp.filePath("fallback"))==directory,"便携版本没有优先使用程序目录");
  const auto repo=temp.filePath("checkout");QDir().mkpath(repo+"/src/editor");QDir().mkpath(repo+"/out/vs2022/Release");
  for(const auto &name:{repo+"/CMakeLists.txt",repo+"/src/editor/main.cpp"}){QFile file(name);check(file.open(QIODevice::WriteOnly),"测试工程标记写入失败");}
  check(shutdown_save_directory(repo+"/out/vs2022/Release",directory)==repo,"开发版没有保存到工程根目录");
  auto read=[&](const QString &name){return load_scene_extension(std::filesystem::path(QDir(directory).filePath(name).toStdWString()),{folder},99);};
  auto color=[](int i){return QColor(i*20,60,90);};
  for(int i=0;i<10;++i){
    state.snapshot.values[0].transform.translation_cm.x=float(i);QImage thumbnail;
    if(i>=3&&i!=5){thumbnail=QImage(256,256,QImage::Format_RGB32);thumbnail.fill(color(i));}
    save_shutdown_scene(directory,*state.document,state.snapshot,thumbnail);
  }
  check(read("temp.dufex").snapshot.values[0].transform.translation_cm.x==9,"最新自动保存无法正常加载");
  for(int i=0;i<=6;++i)check(read(QStringLiteral("temp%1.dufex").arg(i)).snapshot.values[0].transform.translation_cm.x==8-i,"自动保存轮转顺序错误");
  check(QDir(directory).entryList({"*.dufex"},QDir::Files).size()==8,"自动保存没有严格保留八份");
  for(int i=-1;i<=6;++i){
    const int version=8-i;const auto name=i<0?QStringLiteral("temp.dufex.png"):QStringLiteral("temp%1.dufex.png").arg(i);const QImage image(QDir(directory).filePath(name));
    if(version>=3&&version!=5)check(image.size()==QSize(256,256)&&image.pixelColor(20,20)==color(version),"缩略图与轮转的场景版本不一致");
    else check(image.isNull(),"缺少截图的旧场景错误沿用了其他版本的图标");
  }
  const QImage latest(QDir(directory).filePath("temp.dufex.png"));
  auto invalid=state.snapshot;invalid.values[0].transform.general_scale=std::numeric_limits<double>::quiet_NaN();bool failed=false;
  try{save_shutdown_scene(directory,*state.document,invalid);}catch(...){failed=true;}check(failed,"无效场景自动保存没有失败");
  check(read("temp.dufex").snapshot.values[0].transform.translation_cm.x==9&&read("temp0.dufex").snapshot.values[0].transform.translation_cm.x==8,"保存失败破坏旧自动保存");
  check(QImage(QDir(directory).filePath("temp.dufex.png"))==latest,"保存失败破坏已有缩略图");
  save_shutdown_scene(directory,*state.document,state.snapshot);
  check(!QFileInfo::exists(QDir(directory).filePath("temp.dufex.png"))&&QImage(QDir(directory).filePath("temp0.dufex.png"))==latest,"没有新截图时未清除过期图标或丢失上一份截图");
}
