#pragma once
#include <exception>
#include <functional>

namespace dfv::runtime {
struct EvaluationCancelled final:std::exception {const char *what() const noexcept override{return "形变任务已过期";}};
inline thread_local const std::function<bool()> *evaluation_cancelled=nullptr;
class EvaluationCancellation {
  std::function<bool()> cancelled_;
  const std::function<bool()> *previous_;
public:
  explicit EvaluationCancellation(std::function<bool()> cancelled):cancelled_(std::move(cancelled)),previous_(evaluation_cancelled){evaluation_cancelled=&cancelled_;}
  ~EvaluationCancellation(){evaluation_cancelled=previous_;}
  EvaluationCancellation(const EvaluationCancellation &)=delete;
  EvaluationCancellation &operator=(const EvaluationCancellation &)=delete;
};
inline void evaluation_checkpoint(){if(evaluation_cancelled&&(*evaluation_cancelled)())throw EvaluationCancelled{};}
}
