#pragma once
#include <algorithm>
#include <cmath>
#include <span>

namespace dfv::runtime {
struct CurveKnot {double age=0,value=0;};
// PCHIP：单调区间共用切线，仅在极值或平台处将切线置零。
inline double curve_tangent(std::span<const CurveKnot> p,size_t i){
  const size_t n=p.size();
  auto h=[&](size_t k){return p[k+1].age-p[k].age;};
  auto d=[&](size_t k){return (p[k+1].value-p[k].value)/h(k);};
  if(n==2)return d(0);
  if(i>0&&i+1<n){const double a=d(i-1),b=d(i);if(a==0||b==0||std::signbit(a)!=std::signbit(b))return 0;
    const double w1=2*h(i)+h(i-1),w2=h(i)+2*h(i-1);return (w1+w2)/(w1/a+w2/b);
  }
  const size_t k=i==0?0:n-2,j=i==0?1:n-3;const double a=d(k),b=d(j);
  double m=((2*h(k)+h(j))*a-h(k)*b)/(h(k)+h(j));
  if(a==0||std::signbit(m)!=std::signbit(a))return 0;
  if(std::signbit(a)!=std::signbit(b)&&std::abs(m)>3*std::abs(a))m=3*a;
  return m;
}
struct CurveSegment {
  double a,b,c,d,width;
  double value(double u)const{return ((a*u+b)*u+c)*u+d;}
  double derivative(double u)const{return ((3*a*u+2*b)*u+c)/width;}
  double integral(double u)const{return width*u*(d+u*(c/2+u*(b/3+u*a/4)));}
};
inline CurveSegment curve_segment(std::span<const CurveKnot> p,size_t i){
  const double h=p[i+1].age-p[i].age,delta=p[i+1].value-p[i].value;
  const double a=curve_tangent(p,i)*h,b=curve_tangent(p,i+1)*h;
  return {a+b-2*delta,3*delta-2*a-b,a,p[i].value,h};
}
inline double curve_value(std::span<const CurveKnot> p,double age){
  if(age<=p.front().age)return p.front().value;if(age>=p.back().age)return p.back().value;
  const size_t i=size_t(std::upper_bound(p.begin(),p.end(),age,[](double t,const auto &k){return t<k.age;})-p.begin()-1);
  const auto s=curve_segment(p,i);return s.value((age-p[i].age)/s.width);
}
inline double curve_derivative(std::span<const CurveKnot> p,double age){
  if(age<p.front().age||age>=p.back().age)return 0;
  const size_t i=size_t(std::upper_bound(p.begin(),p.end(),age,[](double t,const auto &k){return t<k.age;})-p.begin()-1);
  const auto s=curve_segment(p,i);return s.derivative((age-p[i].age)/s.width);
}
inline double curve_integral(std::span<const CurveKnot> p,double age){
  double sum=0;for(size_t i=0;i+1<p.size()&&age>p[i].age;++i){const auto s=curve_segment(p,i);sum+=s.integral(std::clamp((age-p[i].age)/s.width,0.,1.));}return sum;
}
}
