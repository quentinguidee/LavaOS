#ifndef CALCULATION_CALCULATION_STORE_H
#define CALCULATION_CALCULATION_STORE_H

#include "calculation.h"

namespace Calculation {

class CalculationStore {
public:
  CalculationStore() : m_startIndex(0) {}
  Calculation * calculationAtIndex(int i);
  Calculation * push(const char * text, Poincare::Context * context);
  void deleteCalculationAtIndex(int i);
  void deleteAll();
  int numberOfCalculations();
  void tidy();
  Poincare::Expression ansExpression(Poincare::Context * context);
  static constexpr int k_maxNumberOfCalculations = 15;
private:
  int m_startIndex;
  Calculation m_calculations[k_maxNumberOfCalculations];
};

}

#endif
