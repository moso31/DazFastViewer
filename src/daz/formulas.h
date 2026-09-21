#pragma once
#include "runtime/formula.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <unordered_map>

namespace dfv::daz {
struct FormulaSource {
  std::vector<std::string> symbols,roots,alias_symbols;
  std::unordered_map<std::string,uint32_t> interned;
  std::vector<runtime::Expression> expressions;
  uint32_t symbol(const std::string &address);
};
void append_formulas(FormulaSource &source,uint32_t owner,const nlohmann::json &formulas,const std::function<std::string(const std::string &)> &address);
struct MorphCatalog;
struct SkinCatalog;
struct FormulaCatalog {std::vector<runtime::FormulaGraph> graphs;nlohmann::json report;};
FormulaCatalog enable_formulas(MorphCatalog &morphs,const SkinCatalog &skins);
}
