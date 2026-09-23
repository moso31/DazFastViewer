#pragma once
#include <QDoubleSpinBox>
#include <QLocale>
#include <charconv>
#include <cmath>
#include <limits>

namespace dfv::editor {
// 以 float 的最短可往返十进制显示，避免先提升到 double 后暴露二进制尾数。
inline double decimal_float(double value) {
  char buffer[64];const auto result=std::to_chars(buffer,buffer+sizeof(buffer),float(value));
  if(result.ec!=std::errc{}) return value;
  double decimal=value;std::from_chars(buffer,result.ptr,decimal);return decimal;
}
class NumericSpinBox final:public QDoubleSpinBox {
  bool float_backed_;
public:
  explicit NumericSpinBox(bool float_backed,QWidget *parent=nullptr):QDoubleSpinBox(parent),float_backed_(float_backed) {}
  void sync(double value) {
    // 百分比换算等可能再引入一个 ULP；保留用户刚提交的等价十进制输入。
    if(float_backed_&&std::abs(value-this->value())<=2*std::numeric_limits<float>::epsilon()*std::abs(value)) return;
    setValue(float_backed_?decimal_float(value):value);
  }
protected:
  QString textFromValue(double value) const override {
    auto text=locale().toString(float_backed_?decimal_float(value):value,'f',decimals());
    const auto dot=locale().decimalPoint();
    if(text.contains(dot)) {while(text.endsWith('0')) text.chop(1);if(text.endsWith(dot)) text.chop(dot.size());}
    return text;
  }
};
}
