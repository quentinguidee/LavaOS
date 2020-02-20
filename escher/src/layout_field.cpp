#include <escher/layout_field.h>
#include <escher/clipboard.h>
#include <escher/text_field.h>
#include <poincare/expression.h>
#include <poincare/horizontal_layout.h>
#include <assert.h>
#include <string.h>

using namespace Poincare;

static inline KDCoordinate minCoordinate(KDCoordinate x, KDCoordinate y) { return x < y ? x : y; }

LayoutField::ContentView::ContentView() :
  m_cursor(),
  m_expressionView(0.0f, 0.5f, Palette::PrimaryText, Palette::BackgroundHard, &m_selectionStart, &m_selectionEnd),
  m_cursorView(),
  m_selectionStart(),
  m_selectionEnd(),
  m_isEditing(false)
{
  clearLayout();
}

bool LayoutField::ContentView::setEditing(bool isEditing) {
  m_isEditing = isEditing;
  markRectAsDirty(bounds());
  if (isEditing) {
    /* showEmptyLayoutIfNeeded is done in LayoutField::handleEvent, so no need
     * to do it here. */
    if (m_cursor.hideEmptyLayoutIfNeeded()) {
      m_expressionView.layout().invalidAllSizesPositionsAndBaselines();
      return true;
    }
  }
  layoutSubviews();
  markRectAsDirty(bounds());
  return false;
}

void LayoutField::ContentView::clearLayout() {
  HorizontalLayout h = HorizontalLayout::Builder();
  if (m_expressionView.setLayout(h)) {
    m_cursor.setLayout(h);
  }
}

KDSize LayoutField::ContentView::minimalSizeForOptimalDisplay() const {
  KDSize evSize = m_expressionView.minimalSizeForOptimalDisplay();
  return KDSize(evSize.width() + Poincare::LayoutCursor::k_cursorWidth, evSize.height());
}

bool IsBefore(Layout& l1, Layout& l2, bool strict) {
  char * node1 = reinterpret_cast<char *>(l1.node());
  char * node2 = reinterpret_cast<char *>(l2.node());
  return strict ? (node1 < node2) : (node1 <= node2);
}

void LayoutField::ContentView::addSelection(Layout addedLayout) {
  KDRect rectBefore = selectionRect();
  if (selectionIsEmpty()) {
    /*
     *  ----------  -> +++ is the previous previous selection
     *     (   )    -> added selection
     *  ---+++++--  -> next selection
     * */
    m_selectionStart = addedLayout;
    m_selectionEnd = addedLayout;
  } else if (IsBefore(m_selectionEnd, addedLayout, true)) {
    /*
     *  +++-------  -> +++ is the previous previous selection
     *       (   )  -> added selection
     *  ++++++++++  -> next selection
     *  */
    if (addedLayout.parent() == m_selectionStart) {
      /* The previous selected layout is an horizontal layout and we remove one
       * of its children. */
      assert(m_selectionStart == m_selectionEnd
          && m_selectionStart.type() == LayoutNode::Type::HorizontalLayout);
      m_selectionStart = m_selectionStart.childAtIndex(0);
      m_selectionEnd = m_selectionEnd.childAtIndex(m_selectionEnd.numberOfChildren() - 1);
      addSelection(addedLayout);
      return;
    }
    /* The previous selected layouts and the new added selection are all
     * children of a same horizontal layout. */
    assert(m_selectionStart.parent() == m_selectionEnd.parent()
        && m_selectionStart.parent() == addedLayout.parent()
        && m_selectionStart.parent().type() == LayoutNode::Type::HorizontalLayout);
    m_selectionEnd = addedLayout;
  } else if (IsBefore(addedLayout, m_selectionStart, true)) {
    /*
     *  -------+++  -> +++ is the previous previous selection
     *  (   )       -> added selection
     *  ++++++++++  -> next selection
     * */
    if (addedLayout.type() == LayoutNode::Type::HorizontalLayout
        && m_selectionStart.parent() == addedLayout)
    {
      /* The selection was from the first to the last child of an horizontal
       * layout, we add this horizontal layout -> the selection is now empty. */
      assert(m_selectionEnd.parent() == addedLayout);
      assert(addedLayout.childAtIndex(0) == m_selectionStart);
      assert(addedLayout.childAtIndex(addedLayout.numberOfChildren() - 1) == m_selectionEnd);
      m_selectionStart = Layout();
      m_selectionEnd = Layout();
    } else {
      if (m_selectionStart.hasAncestor(addedLayout, true)) {
        // We are selecting a layout containing the current selection
        m_selectionEnd = addedLayout;
      }
      m_selectionStart = addedLayout;
    }
  } else {
    bool sameEnd = m_selectionEnd == addedLayout;
    bool sameStart = m_selectionStart == addedLayout;
    if (sameStart && sameEnd) {
      /*
       *  -----+++++  -> +++ is the previous previous selection
       *       (   )  -> added selection
       *  ----------  -> next selection
       * */
      m_selectionStart = Layout();
      m_selectionEnd = Layout();
    } else {
      assert(sameStart || sameEnd);
      /*
       *  ++++++++++  -> +++ is the previous previous selection
       *  (   )       -> added selection if sameStart
       *       (   )  -> added selection if sameEnd
       *  +++++-----  -> next selection
       *  The previous selected layouts and the new "added" selection are all
       *  children of a same horizontal layout. */
      Layout horizontalParent = m_selectionStart.parent();
      assert(!horizontalParent.isUninitialized()
          && horizontalParent == m_selectionEnd.parent()
          && horizontalParent == addedLayout.parent()
          && horizontalParent.type() == LayoutNode::Type::HorizontalLayout
          && ((sameEnd && horizontalParent.indexOfChild(m_selectionEnd) > 0)
            || (sameStart && horizontalParent.indexOfChild(m_selectionStart) < horizontalParent.numberOfChildren())));
      if (sameStart) {
        m_selectionStart = horizontalParent.childAtIndex(horizontalParent.indexOfChild(m_selectionStart) + 1);
      } else {
        m_selectionEnd = horizontalParent.childAtIndex(horizontalParent.indexOfChild(m_selectionEnd) - 1);
      }
    }
  }

  KDRect rectAfter = selectionRect();
  // We need to update the background color for selected/unselected layouts
  markRectAsDirty(rectBefore.unionedWith(rectAfter));
}

bool LayoutField::ContentView::resetSelection() {
  if (selectionIsEmpty()) {
    return false;
  }
  m_selectionStart = Layout();
  m_selectionEnd = Layout();
  return true;
}

void LayoutField::ContentView::copySelection(Context * context) {
  if (selectionIsEmpty()) {
    return;
  }
  constexpr int bufferSize = TextField::maxBufferSize();
  char buffer[bufferSize];

  if (m_selectionStart == m_selectionEnd) {
    m_selectionStart.serializeParsedExpression(buffer, bufferSize, context);
    if (buffer[0] == 0) {
      int offset = 0;
      if (m_selectionStart.type() == LayoutNode::Type::VerticalOffsetLayout) {
        assert(bufferSize > 1);
        buffer[offset++] = UCodePointEmpty;
      }
      m_selectionStart.serializeForParsing(buffer + offset, bufferSize - offset);
    }
  } else {
    Layout selectionParent = m_selectionStart.parent();
    assert(!selectionParent.isUninitialized());
    assert(selectionParent.type() == LayoutNode::Type::HorizontalLayout);
    int firstIndex = selectionParent.indexOfChild(m_selectionStart);
    int lastIndex = selectionParent.indexOfChild(m_selectionEnd);
    static_cast<HorizontalLayout&>(selectionParent).serializeChildren(firstIndex, lastIndex, buffer, bufferSize);
  }
  if (buffer[0] != 0) {
    Clipboard::sharedClipboard()->store(buffer);
  }
}

bool LayoutField::ContentView::selectionIsEmpty() const {
  assert(!m_selectionStart.isUninitialized() || m_selectionEnd.isUninitialized());
  assert(!m_selectionEnd.isUninitialized() || m_selectionStart.isUninitialized());
  return m_selectionStart.isUninitialized();
}

void LayoutField::ContentView::deleteSelection() {
  assert(!selectionIsEmpty());
  Layout selectionParent = m_selectionStart.parent();

  /* If the selected layout is the upmost layout, it must be an horizontal
   * layout. Empty it. */
  if (selectionParent.isUninitialized()) {
    assert(m_selectionStart == m_selectionEnd);
    assert(m_selectionStart.type() == LayoutNode::Type::HorizontalLayout);
    clearLayout();
  } else {
    assert(selectionParent == m_selectionEnd.parent());
    // Remove the selected children or replace it with an empty layout.
    if (selectionParent.type() == LayoutNode::Type::HorizontalLayout) {
      int firstIndex = selectionParent.indexOfChild(m_selectionStart);
      int lastIndex = m_selectionStart == m_selectionEnd ? firstIndex : selectionParent.indexOfChild(m_selectionEnd);
      for (int i = lastIndex; i >= firstIndex; i--) {
        static_cast<HorizontalLayout&>(selectionParent).removeChildAtIndex(i, &m_cursor, false);
      }
    } else {
      // Only one child can be selected
      assert(m_selectionStart == m_selectionEnd);
      selectionParent.replaceChildWithEmpty(m_selectionStart, &m_cursor);
    }
  }
  resetSelection();
}

View * LayoutField::ContentView::subviewAtIndex(int index) {
  assert(0 <= index && index < numberOfSubviews());
  View * m_views[] = {&m_expressionView, &m_cursorView};
  return m_views[index];
}

void LayoutField::ContentView::layoutSubviews(bool force) {
  m_expressionView.setFrame(bounds(), force);
  layoutCursorSubview(force);
}

void LayoutField::ContentView::layoutCursorSubview(bool force) {
  if (!m_isEditing) {
    m_cursorView.setFrame(KDRectZero, force);
    return;
  }
  KDPoint expressionViewOrigin = m_expressionView.absoluteDrawingOrigin();
  Layout pointedLayoutR = m_cursor.layout();
  LayoutCursor::Position cursorPosition = m_cursor.position();
  LayoutCursor eqCursor = pointedLayoutR.equivalentCursor(&m_cursor);
  if (eqCursor.isDefined() && pointedLayoutR.hasChild(eqCursor.layout())) {
    pointedLayoutR = eqCursor.layout();
    cursorPosition = eqCursor.position();
  }
  KDPoint cursoredExpressionViewOrigin = pointedLayoutR.absoluteOrigin();
  KDCoordinate cursorX = expressionViewOrigin.x() + cursoredExpressionViewOrigin.x();
  if (cursorPosition == LayoutCursor::Position::Right) {
    cursorX += pointedLayoutR.layoutSize().width();
  }
  if (selectionIsEmpty()) {
    KDPoint cursorTopLeftPosition(cursorX, expressionViewOrigin.y() + cursoredExpressionViewOrigin.y() + pointedLayoutR.baseline() - m_cursor.baselineWithoutSelection());
    m_cursorView.setFrame(KDRect(cursorTopLeftPosition, LayoutCursor::k_cursorWidth, m_cursor.cursorHeightWithoutSelection()), force);
  } else {
    KDRect cursorRect = selectionRect();
    KDPoint cursorTopLeftPosition(cursorX, expressionViewOrigin.y() + cursorRect.y());
    m_cursorView.setFrame(KDRect(cursorTopLeftPosition, LayoutCursor::k_cursorWidth, cursorRect.height()), force);
  }
}

KDRect LayoutField::ContentView::selectionRect() const {
  if (selectionIsEmpty()) {
    return KDRectZero;
  }
  if (m_selectionStart == m_selectionEnd) {
    return KDRect(m_selectionStart.absoluteOrigin(), m_selectionStart.layoutSize());
  }
  Layout selectionParent = m_selectionStart.parent();
  assert(m_selectionEnd.parent() == selectionParent);
  assert(selectionParent.type() == LayoutNode::Type::HorizontalLayout);
  KDRect selectionRectInParent = static_cast<HorizontalLayout &>(selectionParent).relativeSelectionRect(&m_selectionStart, &m_selectionEnd);
  return selectionRectInParent.translatedBy(selectionParent.absoluteOrigin());
}

void LayoutField::setEditing(bool isEditing) {
  KDSize previousLayoutSize = m_contentView.minimalSizeForOptimalDisplay();
  if (m_contentView.setEditing(isEditing)) {
    reload(previousLayoutSize);
  }
}

Context * LayoutField::context() const {
  return (m_delegate != nullptr) ? m_delegate->context() : nullptr;
}

CodePoint LayoutField::XNTCodePoint(CodePoint defaultXNTCodePoint) {
  CodePoint xnt = m_contentView.cursor()->layout().XNTCodePoint();
  if (xnt != UCodePointNull) {
    return xnt;
  }
  return defaultXNTCodePoint;
}

void LayoutField::putCursorRightOfLayout() {
  m_contentView.cursor()->layout().removeGreySquaresFromAllMatrixAncestors();
  m_contentView.setCursor(LayoutCursor(m_contentView.expressionView()->layout(), LayoutCursor::Position::Right));
}

void LayoutField::reload(KDSize previousSize) {
  layout().invalidAllSizesPositionsAndBaselines();
  KDSize newSize = minimalSizeForOptimalDisplay();
  if (m_delegate && previousSize.height() != newSize.height()) {
    m_delegate->layoutFieldDidChangeSize(this);
  }
  m_contentView.cursorPositionChanged();
  scrollToCursor();
  markRectAsDirty(bounds());
}

bool LayoutField::handleEventWithText(const char * text, bool indentation, bool forceCursorRightOfText) {
  /* The text here can be:
   * - the result of a key pressed, such as "," or "cos(•)"
   * - the text added after a toolbox selection
   * - the result of a copy-paste. */

  // Delete the selected layouts if needed
  if (!m_contentView.selectionIsEmpty()) {
    deleteSelection();
  }

  if (text[0] == 0) {
    // The text is empty
    return true;
  }

  int currentNumberOfLayouts = m_contentView.expressionView()->numberOfLayouts();
  if (currentNumberOfLayouts >= k_maxNumberOfLayouts - 6) {
    /* We add -6 because in some cases (Ion::Events::Division,
     * Ion::Events::Exp...) we let the layout cursor handle the layout insertion
     * and these events may add at most 6 layouts (e.g *10^). */
    return true;
  }

  // Handle special cases
  if (strcmp(text, Ion::Events::Division.text()) == 0) {
    m_contentView.cursor()->addFractionLayoutAndCollapseSiblings();
  } else if (strcmp(text, Ion::Events::Exp.text()) == 0) {
    m_contentView.cursor()->addEmptyExponentialLayout();
  } else if (strcmp(text, Ion::Events::Power.text()) == 0) {
    m_contentView.cursor()->addEmptyPowerLayout();
  } else if (strcmp(text, Ion::Events::Sqrt.text()) == 0) {
    m_contentView.cursor()->addRoot();
  } else if (strcmp(text, Ion::Events::Square.text()) == 0) {
    m_contentView.cursor()->addEmptySquarePowerLayout();
  } else if (strcmp(text, Ion::Events::EE.text()) == 0) {
    m_contentView.cursor()->addEmptyTenPowerLayout();
  } else if ((strcmp(text, "[") == 0) || (strcmp(text, "]") == 0)) {
    m_contentView.cursor()->addEmptyMatrixLayout();
  } else if((strcmp(text, Ion::Events::Multiplication.text())) == 0){
    m_contentView.cursor()->addMultiplicationPointLayout();
  } else {
    Expression resultExpression = Expression::Parse(text, nullptr);
    if (resultExpression.isUninitialized()) {
      // The text is not parsable (for instance, ",") and is added char by char.
      KDSize previousLayoutSize = minimalSizeForOptimalDisplay();
      m_contentView.cursor()->insertText(text, forceCursorRightOfText);
      reload(previousLayoutSize);
      return true;
    }
    // The text is parsable, we create its layout an insert it.
    Layout resultLayout = resultExpression.createLayout(Poincare::Preferences::sharedPreferences()->displayMode(), Poincare::PrintFloat::k_numberOfStoredSignificantDigits);
    if (currentNumberOfLayouts + resultLayout.numberOfDescendants(true) >= k_maxNumberOfLayouts) {
      return true;
    }
    insertLayoutAtCursor(resultLayout, resultExpression, forceCursorRightOfText);
  }
  return true;
}

bool LayoutField::shouldFinishEditing(Ion::Events::Event event) {
  if (m_delegate->layoutFieldShouldFinishEditing(this, event)) {
    resetSelection();
    return true;
  }
  return false;
}

bool LayoutField::handleEvent(Ion::Events::Event event) {
  bool didHandleEvent = false;
  KDSize previousSize = minimalSizeForOptimalDisplay();
  bool shouldRecomputeLayout = m_contentView.cursor()->showEmptyLayoutIfNeeded();
  bool moveEventChangedLayout = false;
  if (privateHandleMoveEvent(event, &moveEventChangedLayout)) {
    if (!isEditing()) {
      setEditing(true);
    }
    shouldRecomputeLayout = shouldRecomputeLayout || moveEventChangedLayout;
    didHandleEvent = true;
  } else if (privateHandleSelectionEvent(event, &shouldRecomputeLayout)) {
    didHandleEvent = true;
    // Handle matrices
    if (!m_contentView.selectionIsEmpty()) {
      bool removedSquares = false;
      Layout * selectStart = m_contentView.selectionStart();
      Layout * selectEnd = m_contentView.selectionEnd();
      if (*selectStart != *selectEnd) {
        Layout p = selectStart->parent();
        assert(p == selectEnd->parent());
        assert(p.type() == LayoutNode::Type::HorizontalLayout);
        removedSquares = p.removeGreySquaresFromAllMatrixChildren();
      } else {
        removedSquares = selectStart->removeGreySquaresFromAllMatrixChildren();
      }
      shouldRecomputeLayout = m_contentView.cursor()->layout().removeGreySquaresFromAllMatrixChildren() || removedSquares || shouldRecomputeLayout;
    }
  } else if (privateHandleEvent(event)) {
    shouldRecomputeLayout = true;
    didHandleEvent = true;
  }
  /* Hide empty layout only if the layout is being edited, otherwise the cursor
   * is not visible so any empty layout should be visible. */
  bool didHideLayouts = isEditing() && m_contentView.cursor()->hideEmptyLayoutIfNeeded();
  if (!didHandleEvent) {
    return false;
  }
  shouldRecomputeLayout = didHideLayouts || shouldRecomputeLayout;
  if (!shouldRecomputeLayout) {
    m_contentView.cursorPositionChanged();
    scrollToCursor();
  } else {
    reload(previousSize);
  }
  return true;
}

void LayoutField::deleteSelection() {
  m_contentView.deleteSelection();
}

bool LayoutField::privateHandleEvent(Ion::Events::Event event) {
  if (m_delegate && m_delegate->layoutFieldDidReceiveEvent(this, event)) {
    return true;
  }
  if (handleBoxEvent(event)) {
    if (!isEditing()) {
      setEditing(true);
    }
    return true;
  }
  if (isEditing() && m_delegate && m_delegate->layoutFieldShouldFinishEditing(this, event)) { //TODO use class method?
    setEditing(false);
    if (m_delegate->layoutFieldDidFinishEditing(this, layout(), event)) {
      // Reinit layout for next use
      clearLayout();
      resetSelection();
    } else {
      setEditing(true);
    }
    return true;
  }
  /* if move event was not caught neither by privateHandleMoveEvent nor by
   * layoutFieldShouldFinishEditing, we handle it here to avoid bubbling the
   * event up. */
  if ((event == Ion::Events::Up || event == Ion::Events::Down || event == Ion::Events::Left || event == Ion::Events::Right) && isEditing()) {
    return true;
  }
  if ((event == Ion::Events::OK || event == Ion::Events::EXE) && !isEditing()) {
    setEditing(true);
    m_contentView.cursor()->setLayout(layout());
    m_contentView.cursor()->setPosition(LayoutCursor::Position::Right);
    return true;
  }
  if (event == Ion::Events::Back && isEditing()) {
    clearLayout();
    resetSelection();
    setEditing(false);
    m_delegate->layoutFieldDidAbortEditing(this);
    return true;
  }
  if (event.hasText() || event == Ion::Events::Paste || event == Ion::Events::Backspace) {
    if (!isEditing()) {
      setEditing(true);
    }
    if (event.hasText()) {
      if(event.text() == "%" && Ion::Events::isLockActive() ){
        m_contentView.cursor()->performBackspace();
      } else {
        handleEventWithText(event.text());
      }
    } else if (event == Ion::Events::Paste) {
      handleEventWithText(Clipboard::sharedClipboard()->storedText(), false, true);
    } else {
      assert(event == Ion::Events::Backspace);
      if (!m_contentView.selectionIsEmpty()) {
        deleteSelection();
      } else {
        m_contentView.cursor()->performBackspace();
      }
    }
    return true;
  }
  if (event == Ion::Events::Copy && isEditing()) {
    m_contentView.copySelection(context());
    return true;
  }
  if (event == Ion::Events::Clear && isEditing()) {
    clearLayout();
    resetSelection();
    return true;
  }
  return false;
}

static inline bool IsSimpleMoveEvent(Ion::Events::Event event) {
  return event == Ion::Events::Left
    || event == Ion::Events::Right
    || event == Ion::Events::Up
    || event == Ion::Events::Down;
}

bool LayoutField::privateHandleMoveEvent(Ion::Events::Event event, bool * shouldRecomputeLayout) {
  if (!IsSimpleMoveEvent(event)) {
    return false;
  }
  if (resetSelection()) {
    *shouldRecomputeLayout = true;
    return true;
  }
  LayoutCursor result;
  if (event == Ion::Events::Left) {
    result = m_contentView.cursor()->cursorAtDirection(LayoutCursor::Direction::Left, shouldRecomputeLayout);
  } else if (event == Ion::Events::Right) {
    result = m_contentView.cursor()->cursorAtDirection(LayoutCursor::Direction::Right, shouldRecomputeLayout);
  } else if (event == Ion::Events::Up) {
    result = m_contentView.cursor()->cursorAtDirection(LayoutCursor::Direction::Up, shouldRecomputeLayout);
  } else {
    assert(event == Ion::Events::Down);
    result = m_contentView.cursor()->cursorAtDirection(LayoutCursor::Direction::Down, shouldRecomputeLayout);
  }
  if (result.isDefined()) {
    m_contentView.setCursor(result);
    return true;
  }
  return false;
}

bool eventIsSelection(Ion::Events::Event event) {
  return event == Ion::Events::ShiftLeft || event == Ion::Events::ShiftRight || event == Ion::Events::ShiftUp || event == Ion::Events::ShiftDown;
}

bool LayoutField::privateHandleSelectionEvent(Ion::Events::Event event, bool * shouldRecomputeLayout) {
  if (!eventIsSelection(event)) {
    return false;
  }
  Layout addedSelection;
  LayoutCursor::Direction direction = event == Ion::Events::ShiftLeft ? LayoutCursor::Direction::Left :
    (event == Ion::Events::ShiftRight ? LayoutCursor::Direction::Right :
     (event == Ion::Events::ShiftUp ? LayoutCursor::Direction::Up :
      LayoutCursor::Direction::Down));
  LayoutCursor result = m_contentView.cursor()->selectAtDirection(direction, shouldRecomputeLayout, &addedSelection);
  if (addedSelection.isUninitialized()) {
    return false;
  }
  m_contentView.addSelection(addedSelection);
  assert(result.isDefined());
  m_contentView.setCursor(result);
  return true;
}

void LayoutField::scrollRightOfLayout(Layout layoutR) {
  KDRect layoutRect(layoutR.absoluteOrigin().translatedBy(m_contentView.expressionView()->drawingOrigin()), layoutR.layoutSize());
  scrollToBaselinedRect(layoutRect, layoutR.baseline());
}

void LayoutField::scrollToBaselinedRect(KDRect rect, KDCoordinate baseline) {
  scrollToContentRect(rect, true);
  // Show the rect area around its baseline
  KDCoordinate underBaseline = rect.height() - baseline;
  KDCoordinate minAroundBaseline = minCoordinate(baseline, underBaseline);
  minAroundBaseline = minCoordinate(minAroundBaseline, bounds().height() / 2);
  KDRect balancedRect(rect.x(), rect.y() + baseline - minAroundBaseline, rect.width(), 2 * minAroundBaseline);
  scrollToContentRect(balancedRect, true);
}

void LayoutField::insertLayoutAtCursor(Layout layoutR, Poincare::Expression correspondingExpression, bool forceCursorRightOfLayout) {
  if (layoutR.isUninitialized()) {
    return;
  }

  KDSize previousSize = minimalSizeForOptimalDisplay();
  Poincare::LayoutCursor * cursor = m_contentView.cursor();

  // Handle empty layouts
  cursor->showEmptyLayoutIfNeeded();

  bool layoutWillBeMerged = layoutR.type() == LayoutNode::Type::HorizontalLayout;
  Layout lastMergedLayoutChild = (layoutWillBeMerged && layoutR.numberOfChildren() > 0) ? layoutR.childAtIndex(layoutR.numberOfChildren()-1) : Layout();

  // If the layout will be merged, find now where the cursor will point
  assert(!correspondingExpression.isUninitialized());
  Layout cursorMergedLayout = Layout();
  if (layoutWillBeMerged) {
    if (forceCursorRightOfLayout) {
      cursorMergedLayout = lastMergedLayoutChild;
    } else {
      cursorMergedLayout = layoutR.layoutToPointWhenInserting(&correspondingExpression);
      if (cursorMergedLayout == layoutR) {
        /* LayoutR will not be inserted in the layout, so point to its last
         * child instead. It is visually equivalent. */
        cursorMergedLayout = lastMergedLayoutChild;
      }
    }
  }

  // Add the layout. This puts the cursor at the right of the added layout
  cursor->addLayoutAndMoveCursor(layoutR);

  /* Move the cursor if needed.
   *
   * If forceCursorRightOfLayout is true and the layout has been merged, there
   * is no need to move the cursor because it already points to the right of the
   * added layouts.
   *
   * If the layout to point to has been merged, only its children have been
   * inserted in the layout. We already computed where the cursor should point,
   * because we cannot compute this now that the children are merged in between
   * another layout's children.
   *
   * For other cases, move the cursor to the layout indicated by
   * layoutToPointWhenInserting. This pointed layout cannot be computed before
   * adding layoutR, because addLayoutAndMoveCursor might have changed layoutR's
   * children.
   * For instance, if we add an absolute value with an empty child left of a 0,
   * the empty child is deleted and the 0 is collapsed into the absolute value.
   * Sketch of the situation, ' being the cursor:
   *  Initial layout:   '0
   *  "abs(x)" pressed in the toolbox => |•| is added, • being an empty layout
   *  Final layout: |0'|
   *
   * Fortunately, merged layouts' children are not modified by the merge, so it
   * is ok to compute their pointed layout before adding them.
   * */

  if (!forceCursorRightOfLayout) {
    if (!layoutWillBeMerged) {
      assert(cursorMergedLayout.isUninitialized());
      assert(!correspondingExpression.isUninitialized());
      cursorMergedLayout = layoutR.layoutToPointWhenInserting(&correspondingExpression);
    }
    assert(!cursorMergedLayout.isUninitialized());
    m_contentView.cursor()->setLayout(cursorMergedLayout);
    m_contentView.cursor()->setPosition(LayoutCursor::Position::Right);
  } else if (!layoutWillBeMerged) {
    m_contentView.cursor()->setLayout(layoutR);
    m_contentView.cursor()->setPosition(LayoutCursor::Position::Right);
  }

  // Handle matrices
  cursor->layout().addGreySquaresToAllMatrixAncestors();

  // Handle empty layouts
  cursor->hideEmptyLayoutIfNeeded();

  // Reload
  reload(previousSize);
  if (!layoutWillBeMerged) {
    scrollRightOfLayout(layoutR);
  } else {
    assert(!lastMergedLayoutChild.isUninitialized());
    scrollRightOfLayout(lastMergedLayoutChild);
  }
  scrollToCursor();
}
