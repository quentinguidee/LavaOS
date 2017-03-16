#ifndef STATISTICS_CALCULATION_CONTROLLER_H
#define STATISTICS_CALCULATION_CONTROLLER_H

#include <escher.h>
#include "store.h"
#include "../shared/tab_table_controller.h"


namespace Statistics {

class CalculationController : public Shared::TabTableController, public ButtonRowDelegate, public AlternateEmptyViewDelegate {

public:
  CalculationController(Responder * parentResponder, ButtonRowController * header, Store * store);
  const char * title() const override;
  bool handleEvent(Ion::Events::Event event) override;
  void didBecomeFirstResponder() override;

  bool isEmpty() const override;
  const char * emptyMessage() override;
  Responder * defaultController() override;

  int numberOfRows() override;
  int numberOfColumns() override;
  void willDisplayCellAtLocation(HighlightCell * cell, int i, int j) override;
  KDCoordinate columnWidth(int i) override;
  KDCoordinate rowHeight(int j) override;
  HighlightCell * reusableCell(int index, int type) override;
  int reusableCellCount(int type) override;
  int typeAtLocation(int i, int j) override;
private:
  Responder * tabController() const override;
  constexpr static int k_totalNumberOfRows = 13;
  constexpr static int k_maxNumberOfDisplayableRows = 11;
  static constexpr KDCoordinate k_cellHeight = 20;
  static constexpr KDCoordinate k_cellWidth = Ion::Display::Width/2 - Metric::CommonRightMargin/2 - Metric::CommonLeftMargin/2;
  EvenOddPointerTextCell m_titleCells[k_maxNumberOfDisplayableRows];
  EvenOddBufferTextCell m_calculationCells[k_maxNumberOfDisplayableRows];
  Store * m_store;
};

}


#endif
