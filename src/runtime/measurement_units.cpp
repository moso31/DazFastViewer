#include "runtime/measurement_units.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace dfv::runtime {
namespace {
// 有限十进制的整数有效位与十进制指数；乘法和最终舍入均不依赖浮点范围。
struct Decimal {
  std::string digits="0";int exponent=0;
  void normalize(){const auto first=digits.find_first_not_of('0');if(first==std::string::npos){digits="0";exponent=0;return;}digits.erase(0,first);while(digits.size()>1&&digits.back()=='0'){digits.pop_back();++exponent;}}
  int order()const{return digits=="0"?-100000:int(digits.size())-1+exponent;}
  static Decimal parse(const std::string &s){
    if(s.empty()||s.size()>128)throw std::runtime_error("请输入正数倍率，支持小数或科学计数法，最多 128 个字符");
    Decimal d;d.digits.clear();size_t i=s[0]=='+'?1:0;bool dot=false,any=false;
    for(;i<s.size()&&s[i]!='e'&&s[i]!='E';++i){const char c=s[i];if(c=='.'&&!dot){dot=true;continue;}if(c<'0'||c>'9')throw std::runtime_error("倍率必须为正数，可使用 1e6 这样的科学计数法");d.digits+=c;any=true;if(dot)--d.exponent;}
    if(!any)throw std::runtime_error("倍率缺少数字");
    if(i<s.size()){++i;bool negative=false;if(i<s.size()&&(s[i]=='+'||s[i]=='-'))negative=s[i++]=='-';if(i==s.size())throw std::runtime_error("倍率指数缺少数字");int exp=0;for(;i<s.size();++i){if(s[i]<'0'||s[i]>'9'||exp>10000)throw std::runtime_error("倍率指数范围为 -10000 到 10000");exp=exp*10+s[i]-'0';}if(exp>10000)throw std::runtime_error("倍率指数范围为 -10000 到 10000");d.exponent+=negative?-exp:exp;}
    d.normalize();return d;
  }
  static Decimal from(double value){if(!std::isfinite(value)||value<0)throw std::runtime_error("测量结果必须是非负有限值");char buffer[64];const auto r=std::to_chars(buffer,buffer+sizeof(buffer),value);if(r.ec!=std::errc{})throw std::runtime_error("测量结果转换失败");return parse(std::string(buffer,r.ptr));}
  Decimal operator*(const Decimal &b)const{
    if(digits=="0"||b.digits=="0")return {};
    std::vector<int> v(digits.size()+b.digits.size());
    for(size_t i=0;i<digits.size();++i)for(size_t j=0;j<b.digits.size();++j)v[i+j+1]+=(digits[i]-'0')*(b.digits[j]-'0');
    for(size_t i=v.size()-1;i>0;--i){v[i-1]+=v[i]/10;v[i]%=10;}
    Decimal r;r.digits.clear();for(int n:v)r.digits+=char('0'+n);r.exponent=exponent+b.exponent;r.normalize();return r;
  }
  static void increment(std::string &s){for(size_t i=s.size();i>0;--i){if(s[i-1]!='9'){++s[i-1];return;}s[i-1]='0';}s.insert(s.begin(),'1');}
  std::string fixed()const{
    const int shift=exponent+2;std::string s;
    if(shift>=0)s=digits+std::string(size_t(shift),'0');
    else{const int end=int(digits.size())+shift;s=end>0?digits.substr(0,size_t(end)):"0";if(end>=0&&digits[size_t(end)]>='5')increment(s);}
    if(s.size()<3)s.insert(0,3-s.size(),'0');s.insert(s.size()-2,1,'.');while(s.back()=='0')s.pop_back();if(s.back()=='.')s.pop_back();return s;
  }
  std::string display()const{
    if(digits=="0")return "0";int power=order();if(power>=-2&&power<12)return fixed();
    auto mantissa=*this;mantissa.exponent=1-int(digits.size());auto s=mantissa.fixed();if(s=="10"){s="1";++power;}return s+"e"+std::to_string(power);
  }
};
Decimal scale_value(const std::string &s){auto d=Decimal::parse(s);if(d.digits=="0")throw std::runtime_error("测量倍率必须大于零");return d;}
}
std::string measurement_scale(const std::string &s){scale_value(s);return s;}
std::string measurement_height(double centimeters,const std::string &scale){
  auto v=Decimal::from(centimeters)*scale_value(scale);const auto order=v.order();const char *unit=" cm";if(order>=6){v.exponent-=5;unit=" km";}else if(order>=3){v.exponent-=2;unit=" m";}return v.display()+unit;
}
std::string measurement_mass(double kilograms,const std::string &scale){
  const auto factor=scale_value(scale);auto v=Decimal::from(kilograms)*factor*factor*factor;const auto order=v.order();const char *unit=" kg";if(order<0){v.exponent+=3;unit=" g";}else if(order>=3){v.exponent-=3;unit=" t";}return v.display()+unit;
}
}
