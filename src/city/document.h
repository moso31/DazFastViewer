#pragma once
#include "city/generator.h"
namespace dfv::editor {struct Document;struct Snapshot;}
namespace dfv::city {
void install(editor::Document &,Generated,bool record=true);
void remove(editor::Document &,const std::string &id,bool record=true);
void prune(const editor::Document &,editor::Snapshot &);
}
