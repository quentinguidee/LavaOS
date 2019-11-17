#ifndef SETTINGS_EXAM_MODE_CONTROLLER_H
#define SETTINGS_EXAM_MODE_CONTROLLER_H

#include "generic_sub_controller.h"
#include "led_controller.h"

namespace Settings {

class ExamModeController : public GenericSubController {
public:
  ExamModeController(Responder * parentResponder);
  void didEnterResponderChain(Responder * previousFirstResponder) override;
  bool handleEvent(Ion::Events::Event event) override;
  HighlightCell * reusableCell(int index, int type) override;
  int reusableCellCount(int type) override;
  void willDisplayCellForIndex(HighlightCell * cell, int index) override;
  int typeAtLocation(int i, int j) override;
private:
  LEDController m_LEDController;
  MessageTableCell m_examModeCell;
  MessageTableCellWithChevronAndMessage m_ledCell;
};

}

#endif
