#include "SeerEditorWidget.h"
#include "SeerBreakpointCreateDialog.h"
#include "SeerUtl.h"
#include <QtGui/QColor>
#include <QtGui/QPainter>
#include <QtGui/QTextBlock>
#include <QtGui/QFont>
#include <QtGui/QIcon>
#include <QtGui/QRadialGradient>
#include <QtGui/QHelpEvent>
#include <QtGui/QPainterPath>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QMenu>
#include <QtWidgets/QAction>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QMessageBox>
#include <QtGui/QTextCursor>
#include <QtGui/QPalette>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/QDebug>

SeerEditorWidgetSourceArea::SeerEditorWidgetSourceArea(QWidget* parent) : QPlainTextEdit(parent) {

    _enableLineNumberArea       = false;
    _enableBreakPointArea       = false;
    _enableMiniMapArea          = false;
    _ctrlKeyPressed             = false;
    _sourceHighlighter          = 0;
    _sourceHighlighterEnabled   = true;

    QFont font("Source Code Pro");
    font.setStyleHint(QFont::Monospace);
    setFont(font);

    setReadOnly(true);
    setTextInteractionFlags(textInteractionFlags() | Qt::TextSelectableByKeyboard);
    setLineWrapMode(QPlainTextEdit::NoWrap);

    _lineNumberArea = new SeerEditorWidgetLineNumberArea(this);
    _breakPointArea = new SeerEditorWidgetBreakPointArea(this);
    _miniMapArea    = new SeerEditorWidgetMiniMapArea(this);
    _miniMapPixmap  = 0;

    enableLineNumberArea(true);
    enableBreakPointArea(true);
    enableMiniMapArea(false);  // Doesn't work yet. Need to work on the "mini" part.

  //QObject::connect(this, &SeerEditorWidgetSourceArea::cursorPositionChanged,              this,            &SeerEditorWidgetSourceArea::highlightCurrentLine);
    QObject::connect(this, &SeerEditorWidgetSourceArea::blockCountChanged,                  this,            &SeerEditorWidgetSourceArea::updateMarginAreasWidth);
    QObject::connect(this, &SeerEditorWidgetSourceArea::updateRequest,                      this,            &SeerEditorWidgetSourceArea::updateLineNumberArea);
    QObject::connect(this, &SeerEditorWidgetSourceArea::updateRequest,                      this,            &SeerEditorWidgetSourceArea::updateBreakPointArea);
    QObject::connect(this, &SeerEditorWidgetSourceArea::updateRequest,                      this,            &SeerEditorWidgetSourceArea::updateMiniMapArea);
    QObject::connect(this, &SeerEditorWidgetSourceArea::highlighterSettingsChanged,         this,            &SeerEditorWidgetSourceArea::handleHighlighterSettingsChanged);

    setCurrentLine(0);

    updateMarginAreasWidth(0);

    // Calling close() will clear the text document.
    close();
}

void SeerEditorWidgetSourceArea::enableLineNumberArea (bool flag) {
    _enableLineNumberArea = flag;

    updateMarginAreasWidth(0);
}

bool SeerEditorWidgetSourceArea::lineNumberAreaEnabled () const {
    return _enableLineNumberArea;
}

void SeerEditorWidgetSourceArea::enableBreakPointArea (bool flag) {
    _enableBreakPointArea = flag;

    updateMarginAreasWidth(0);
}

bool SeerEditorWidgetSourceArea::breakPointAreaEnabled () const {
    return _enableBreakPointArea;
}

void SeerEditorWidgetSourceArea::enableMiniMapArea (bool flag) {
    _enableMiniMapArea = flag;

    updateMarginAreasWidth(0);
}

bool SeerEditorWidgetSourceArea::miniMapAreaEnabled () const {
    return _enableMiniMapArea;
}

void SeerEditorWidgetSourceArea::updateMarginAreasWidth (int newBlockCount) {

    Q_UNUSED(newBlockCount);

    int leftMarginWidth  = lineNumberAreaWidth() + breakPointAreaWidth();
    int rightMarginWidth = miniMapAreaWidth();

    setViewportMargins(leftMarginWidth, 0, rightMarginWidth, 0);
}

int SeerEditorWidgetSourceArea::lineNumberAreaWidth () {

    if (lineNumberAreaEnabled() == false) {
        return 0;
    }

    int digits = 1;
    int max    = qMax(1, blockCount());

    while (max >= 10) {
        max /= 10;
        digits++;
    }

    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;

    return space;
}

int SeerEditorWidgetSourceArea::breakPointAreaWidth () {

    if (breakPointAreaEnabled() == false) {
        return 0;
    }

    int space = 3 + 20;

    return space;
}

int SeerEditorWidgetSourceArea::miniMapAreaWidth () {

    if (miniMapAreaEnabled() == false) {
        return 0;
    }

    int space = 3 + 75;

    return space;
}

void SeerEditorWidgetSourceArea::updateLineNumberArea (const QRect& rect, int dy) {

    if (lineNumberAreaEnabled() == false) {
        return;
    }

    if (dy) {
        _lineNumberArea->scroll(0, dy);
    }else{
        _lineNumberArea->update(0, rect.y(), _lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateMarginAreasWidth(0);
    }
}

void SeerEditorWidgetSourceArea::updateBreakPointArea (const QRect& rect, int dy) {

    if (breakPointAreaEnabled() == false) {
        return;
    }

    if (dy) {
        _breakPointArea->scroll(0, dy);
    }else{
        _breakPointArea->update(0, rect.y(), _breakPointArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateMarginAreasWidth(0);
    }
}

void SeerEditorWidgetSourceArea::updateMiniMapArea (const QRect& rect, int dy) {

    if (miniMapAreaEnabled() == false) {
        return;
    }

    if (dy) {
        _miniMapArea->scroll(0, dy);
    }else{
        _miniMapArea->update(0, rect.y(), _miniMapArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateMarginAreasWidth(0);
    }
}

void SeerEditorWidgetSourceArea::lineNumberAreaPaintEvent (QPaintEvent* event) {

    if (lineNumberAreaEnabled() == false) {
        return;
    }

    QTextCharFormat format = highlighterSettings().get("Margin");

    QPainter painter(_lineNumberArea);
    painter.fillRect(event->rect(), format.background().color());
    painter.setPen(format.foreground().color());

    QFont font = painter.font();
    font.setItalic(format.fontItalic());
    font.setWeight(format.fontWeight());
    painter.setFont(font);

    QTextBlock block       = firstVisibleBlock();
    int        blockNumber = block.blockNumber();
    int        top         = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int        bottom      = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {

        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.drawText(0, top, _lineNumberArea->width(), fontMetrics().height(), Qt::AlignRight, number);
        }

        block  = block.next();
        top    = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());

        blockNumber++;
    }
}

void SeerEditorWidgetSourceArea::breakPointAreaPaintEvent (QPaintEvent* event) {

    if (breakPointAreaEnabled() == false) {
        return;
    }

    QTextCharFormat format = highlighterSettings().get("Margin");

    QPainter painter(_breakPointArea);
    painter.fillRect(event->rect(), format.background().color());
    painter.setPen(format.foreground().color());

    QFont font = painter.font();
    font.setItalic(format.fontItalic());
    font.setWeight(format.fontWeight());
    painter.setFont(font);

    QTextBlock block       = firstVisibleBlock();
    int        blockNumber = block.blockNumber();
    int        top         = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int        bottom      = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {

        if (block.isVisible() && bottom >= event->rect().top()) {

            if (hasBreakpointLine(blockNumber+1)) {
                if (breakpointLineEnabled(blockNumber+1)) {
                    QRect rect(_breakPointArea->width() - 20, top, fontMetrics().height(), fontMetrics().height());

                    QPainterPath path;
                    path.addEllipse(rect);

                    QPointF bias = QPointF(rect.width() * .25 * 1.0, rect.height() * .25 * -1.0);

                    QRadialGradient gradient(rect.center(), rect.width() / 2.0, rect.center() + bias);
                    gradient.setColorAt(0.0, QColor(Qt::white));
                    gradient.setColorAt(0.9, QColor(Qt::red));
                    gradient.setColorAt(1.0, QColor(Qt::transparent));
                    painter.fillPath(path,QBrush(gradient));

                }else{
                    QRect rect(_breakPointArea->width() - 20, top, fontMetrics().height(), fontMetrics().height());

                    QPainterPath path;
                    path.addEllipse(rect);

                    QPointF bias = QPointF(rect.width() * .25 * 1.0, rect.height() * .25 * -1.0);

                    QRadialGradient gradient(rect.center(), rect.width() / 2.0, rect.center() + bias);
                    gradient.setColorAt(0.0, QColor(Qt::white));
                    gradient.setColorAt(0.9, QColor(Qt::darkGray));
                    gradient.setColorAt(1.0, Qt::transparent);
                    painter.fillPath(path,QBrush(gradient));
                }
            }
        }

        block  = block.next();
        top    = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());

        blockNumber++;
    }
}

void SeerEditorWidgetSourceArea::miniMapAreaPaintEvent (QPaintEvent* event) {

    //
    // This doesn't work yet.
    // There is nothing 'mini' about the view. Need to shrink the text somehow.
    // Then add a 'focus' box that can be interacted with to scroll through the text.
    //

    if (miniMapAreaEnabled() == false) {
        return;
    }

    qDebug() << "Top:" << event->rect().top() << " Right:" << event->rect().right() << " Width:" << event->rect().width() << " Height:" << event->rect().height();

    if (_miniMapPixmap == 0) {

        int pixmapWidth  = 0;
        int pixmapHeight = 0;

        QFont font("Source Code Pro");
        font.setStyleHint(QFont::Monospace);
      //font.setPointSize(2);

        QFontMetrics fm(font);

        {
            QTextBlock block = document()->begin();

            while (block.isValid()) {

                if (fm.horizontalAdvance(block.text()) > pixmapWidth) {
                    pixmapWidth = fm.horizontalAdvance(block.text());
                }

                pixmapHeight += fm.height();

                block = block.next();
            }
        }

        qDebug() << "PIXMAP = " << pixmapWidth << " x " << pixmapHeight;

        QTextCharFormat format = highlighterSettings().get("Margin");

        _miniMapPixmap = new QPixmap(pixmapWidth, pixmapHeight);
        _miniMapPixmap->fill(format.background().color());

        QPainter painter(_miniMapPixmap);
        painter.setPen(format.foreground().color());
        painter.setFont(font);

        QTextBlock block       = document()->begin();
        int        top         = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
        int        bottom      = top + qRound(blockBoundingRect(block).height());

        while (block.isValid()) {

            painter.drawText(0, top, block.text());

            block = block.next();
            top    = bottom;
            bottom = top + qRound(blockBoundingRect(block).height());
        }
    }


    /*
    QRectF target(10.0, 20.0, 80.0, 60.0);
    QRectF source(0.0, 0.0, 70.0, 40.0);

    QRect target(event->rect());
    QRect source(event->rect());

    QPainter painter(_miniMapPixmap);
    painter.drawPixmap(0, 0, *_miniMapPixmap);
    //painter.drawPixmap(target, *_miniMapPixmap, source);

    QPainter painter(_miniMapArea);
    painter.drawPixmap(0, 0, *_miniMapPixmap);
    */

    QFont font("Source Code Pro");
    font.setStyleHint(QFont::Monospace);
  //font.setPointSize(2);

    QTextCharFormat format = highlighterSettings().get("Margin");

    QPainter painter(_miniMapArea);
    painter.fillRect(event->rect(), format.background().color());
    painter.setPen(format.foreground().color());
    painter.setFont(font);

    QTextBlock block       = firstVisibleBlock();
    int        top         = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int        bottom      = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {

        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.drawText(0, top, _miniMapArea->width(), painter.fontMetrics().height(), Qt::AlignLeft, block.text());
        }

        block  = block.next();
        top    = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
      //bottom = top + painter.fontMetrics().height();
    }
}

void SeerEditorWidgetSourceArea::resizeEvent (QResizeEvent* e) {

    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();

    if (lineNumberAreaEnabled()) {
        _lineNumberArea->setGeometry (QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
    }

    if (breakPointAreaEnabled()) {
        _breakPointArea->setGeometry (QRect(cr.left() + lineNumberAreaWidth(), cr.top(), breakPointAreaWidth(), cr.height()));
    }

    if (miniMapAreaEnabled()) {
        _miniMapArea->setGeometry (QRect(cr.right() - miniMapAreaWidth() - verticalScrollBar()->width(), cr.top(), miniMapAreaWidth(), cr.height()));
    }
}

void SeerEditorWidgetSourceArea::contextMenuEvent (QContextMenuEvent* event) {

    showContextMenu(event);
}

void SeerEditorWidgetSourceArea::keyPressEvent (QKeyEvent* event) {

    if (event->key() == Qt::Key_F && event->modifiers() == Qt::ControlModifier) {
        emit showSearchBar(true);
    }

    if (event->key() == Qt::Key_O && event->modifiers() == Qt::ControlModifier) {
        emit showAlternateBar(true);
    }

    QPlainTextEdit::keyPressEvent(event);
}

void SeerEditorWidgetSourceArea::mouseReleaseEvent (QMouseEvent* event) {

    QPlainTextEdit::mouseReleaseEvent(event);

    if (textCursor().selectedText() == "") {
        // Nothing selected, clear current expression.
        clearExpression();
        return;
    }

    _selectedExpressionCursor = textCursor();
    _selectedExpressionValue  = "";
    _selectedExpressionId     = Seer::createID();

    if (_ctrlKeyPressed == true) {
        _selectedExpressionName = QString("*") + textCursor().selectedText();
    }else{
        _selectedExpressionName = textCursor().selectedText();
    }

    emit evaluateVariableExpression(_selectedExpressionId, _selectedExpressionName);
}

bool SeerEditorWidgetSourceArea::event(QEvent* event) {

    // Handle the ToolTip event.
    if (event->type() == QEvent::ToolTip) {

        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);

        // Massage the event location to account for the linenumber and breakpoint widgets.
        QPoint pos = QPoint(helpEvent->pos().x() - _lineNumberArea->width() - _breakPointArea->width(), helpEvent->pos().y());

        // Create a cursor at the position so we can get the text underneath at the cursor.
        QTextCursor cursor = cursorForPosition(pos);
        cursor.select(QTextCursor::WordUnderCursor);

        // If the text isn't empty, display a took tip.
        if (cursor.selectedText().isEmpty() == false && cursor.selectedText() == _selectedExpressionCursor.selectedText()) {
          //QToolTip::showText(helpEvent->globalPos(), QString("%1 = %2").arg(_selectedExpressionName).arg(_selectedExpressionValue) );
            QToolTip::showText(helpEvent->globalPos(), _selectedExpressionValue);

        // Otherwise, hide any old one.
        }else{
            QToolTip::hideText();
        }

        return true;
    }

    // Handle the Key Press/Release event.
    // To handle when Ctrl is pressed or released.
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = (QKeyEvent*)event;

        if (keyEvent->key() == Qt::Key_Control) {
            _ctrlKeyPressed = true;
        }
    }

    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent* keyEvent = (QKeyEvent*)event;

        if (keyEvent->key() == Qt::Key_Control) {
            _ctrlKeyPressed = false;
        }
    }


    // Pass any others to the base class.
    return QPlainTextEdit::event(event);
}

void SeerEditorWidgetSourceArea::highlightCurrentLines () {

    // Any line will be highlighted with a yellow line.
    // The 'yellow' color is for the current line of the most recent stack frame.
    // The 'grey' color is for older stack frames.
    QColor yellowLineColor = highlighterSettings().get("Current Line").background().color();
    QColor grayLineColor   = highlighterSettings().get("Current Line2").background().color();

    // Create an empty list of selections.
    QList<QTextEdit::ExtraSelection> extraSelections;

    // Loop through each highlighted line. Get its text cursor.
    for (int i=0; i<_currentLineCursors.size(); i++) {

        // Create a selection at the cursor.
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground((i == 0 ? yellowLineColor : grayLineColor));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = _currentLineCursors[i];
        selection.cursor.clearSelection();

        // Add it to the list.
        extraSelections.append(selection);
    }

    // Give the editor the list of selections.
    setExtraSelections(extraSelections);
}

void SeerEditorWidgetSourceArea::highlightCurrentLine () {

    QList<QTextEdit::ExtraSelection> extraSelections;

    QColor lineColor = highlighterSettings().get("Current Line").background().color();

    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(lineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();

    extraSelections.append(selection);

    setExtraSelections(extraSelections);
}

void SeerEditorWidgetSourceArea::unhighlightCurrentLine () {

    QList<QTextEdit::ExtraSelection> extraSelections;

    // No need to unhighlight the lines. Creating an empty extraSelections list will do that.
    setExtraSelections(extraSelections);
}

bool SeerEditorWidgetSourceArea::isOpen () const {

    if (_fullname != "") {
        return true;
    }

    return false;
}

void SeerEditorWidgetSourceArea::open (const QString& fullname, const QString& file, const QString& alternateDirectory) {

    // Close the previous file, if any.
    if (isOpen()) {
        close();
    }

    // Save the filename.
    _fullname           = fullname;
    _file               = file;
    _alternateDirectory = alternateDirectory;

    setDocumentTitle(_fullname);

    // If the filename is null, don't do anything.
    if (_fullname == "") {
        return;
    }

    // If the file is null, don't do anything.
    if (_file == "") {
        qDebug() << "'file' is null. Skipping.";
        return;
    }

    //
    // Open the file.
    //
    // Build the filename to open. If there is no alternateDirectory,
    // use fullname.  If alternateDirectory is provided, then use
    //
    //      alternateDirectory/file
    //
    QString filename;

    if (alternateDirectory == "") {
        filename = _fullname;
    }else{
        filename = _alternateDirectory + "/" + _file;
    }

    //qDebug() << "Loading" << _file << "from" << filename;

    QFile inputFile(filename);

    inputFile.open(QIODevice::ReadOnly);

    if (!inputFile.isOpen()) {

        QMessageBox::critical(this, "Can't open source file.",  "Can't open : " + filename + "\nIts location may have changed.");

        emit showAlternateBar(true);

        return;
    }

    // Read the file.
    QTextStream stream(&inputFile);

    QString line = stream.readLine();
    QString text;

    while (!line.isNull()) {

        if (text != "") {
            text += "\n";
        }

        text += line;

        line = stream.readLine();
    };

    // Put the contents in the editor.
    openText(text, _fullname);
}

void SeerEditorWidgetSourceArea::openText (const QString& text, const QString& file) {

    // Put the contents in the editor.
    setPlainText(text);

    // Set the text cursor to the first line.
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor, 1);
    setTextCursor(cursor);

    // Add a syntax highlighter for C++ files.
    if (_sourceHighlighter) {
        delete _sourceHighlighter; _sourceHighlighter = 0;
    }

    QRegExp cpp_re("(?:.c|.cpp|.CPP|.cxx|.CXX|.h|.H|.hpp|.hxx|.Hxx|.HXX)$");
    if (file.contains(cpp_re)) {
        _sourceHighlighter = new SeerCppSourceHighlighter(0);

        if (highlighterEnabled()) {
            _sourceHighlighter->setDocument(document());
        }else{
            _sourceHighlighter->setDocument(0);
        }

        _sourceHighlighter->setHighlighterSettings(_sourceHighlighterSettings);
        _sourceHighlighter->rehighlight();
    }
}

const QString& SeerEditorWidgetSourceArea::fullname () const {
    return _fullname;
}

const QString& SeerEditorWidgetSourceArea::file () const {
    return _file;
}

void SeerEditorWidgetSourceArea::close () {
    setDocumentTitle("");
    setPlainText("");

    clearExpression();
}

void SeerEditorWidgetSourceArea::setAlternateDirectory (const QString& alternateDirectory) {

    _alternateDirectory = alternateDirectory;
}

const QString& SeerEditorWidgetSourceArea::alternateDirectory () const {

    return _alternateDirectory;
}


void SeerEditorWidgetSourceArea::setCurrentLine (int lineno) {

    //qDebug() << lineno << file();

    // Highlight if a valid line number is selected.
    if (lineno < 1) {
        unhighlightCurrentLine();
        return;
    }

    QTextBlock  block  = document()->findBlockByLineNumber(lineno-1);
    QTextCursor cursor = textCursor();

    cursor.setPosition(block.position());
    setTextCursor(cursor);

    highlightCurrentLine();
}

void SeerEditorWidgetSourceArea::scrollToLine (int lineno) {

    //qDebug() << lineno << file();

    // Scroll to the first line if we went before it.
    if (lineno < 1) {
        lineno = 1;
    }

    // Scroll to the last line if we went past it.
    if (lineno > document()->blockCount()) {
        lineno = document()->blockCount();
    }

    QTextBlock  block  = document()->findBlockByLineNumber(lineno-1);
    QTextCursor cursor = textCursor();

    cursor.setPosition(block.position());
    setTextCursor(cursor);

    centerCursor();
}

void SeerEditorWidgetSourceArea::clearCurrentLines () {

    //qDebug() << file();

    _currentLineCursors.resize(0);

    highlightCurrentLines();
}

void SeerEditorWidgetSourceArea::addCurrentLine (int lineno) {

    //qDebug() << lineno << file();

    QTextBlock  block  = document()->findBlockByLineNumber(lineno-1);
    QTextCursor cursor = textCursor();

    cursor.setPosition(block.position());

    _currentLineCursors.push_back(cursor);

    highlightCurrentLines();
}


void SeerEditorWidgetSourceArea::clearBreakpoints () {
    _breakpointsLineNumbers.clear();
    _breakpointsNumbers.clear();
    _breakpointsEnableds.clear();

    repaint();
}

void SeerEditorWidgetSourceArea::addBreakpoint (int number, int lineno, bool enabled) {
    _breakpointsNumbers.push_back(number);
    _breakpointsLineNumbers.push_back(lineno);
    _breakpointsEnableds.push_back(enabled);

    repaint();
}

bool SeerEditorWidgetSourceArea::hasBreakpointNumber (int number) const {
    return _breakpointsLineNumbers.contains(number);
}

bool SeerEditorWidgetSourceArea::hasBreakpointLine (int lineno) const {
    return _breakpointsLineNumbers.contains(lineno);
}

const QVector<int>& SeerEditorWidgetSourceArea::breakpointNumbers () const {
    return _breakpointsNumbers;
}

const QVector<int>& SeerEditorWidgetSourceArea::breakpointLines () const {
    return _breakpointsLineNumbers;
}

const QVector<bool>& SeerEditorWidgetSourceArea::breakpointEnableds () const {
    return _breakpointsEnableds;
}

int SeerEditorWidgetSourceArea::breakpointLineToNumber (int lineno) const {

    // Map lineno to breakpoint number.
    int i = _breakpointsLineNumbers.indexOf(lineno);

    if (i < 0) {
        return 0;
    }

    return _breakpointsNumbers[i];
}

bool SeerEditorWidgetSourceArea::breakpointLineEnabled (int lineno) const {

    // Look for the lineno and get its index.
    int i = _breakpointsLineNumbers.indexOf(lineno);

    // Not found, return false.
    if (i < 0) {
        return false;
    }

    // Otherwise, return the proper status.
    return _breakpointsEnableds[i];
}

void SeerEditorWidgetSourceArea::showContextMenu (QMouseEvent* event) {

    showContextMenu(event->pos(), event->globalPos());
}

void SeerEditorWidgetSourceArea::showContextMenu (QContextMenuEvent* event) {

    showContextMenu(event->pos(), event->globalPos());
}

void SeerEditorWidgetSourceArea::showContextMenu (const QPoint& pos, const QPoint& globalPos) {

    // Get the line number for the cursor position.
    QTextCursor cursor = cursorForPosition(pos);

    int lineno = cursor.blockNumber()+1;

    // Create the menu actions.
    QAction* createAction;
    QAction* deleteAction;
    QAction* enableAction;
    QAction* disableAction;
    QAction* runToLineAction;
    QAction* addVariableLoggerExpressionAction;
    QAction* addVariableLoggerAsteriskExpressionAction;
    QAction* addVariableLoggerAmpersandExpressionAction;
    QAction* addVariableLoggerAsteriskAmpersandExpressionAction;
    QAction* addVariableTrackerExpressionAction;
    QAction* addVariableTrackerAsteriskExpressionAction;
    QAction* addVariableTrackerAmpersandExpressionAction;
    QAction* addVariableTrackerAsteriskAmpersandExpressionAction;
    QAction* addMemoryVisualizerAction;
    QAction* addMemoryAsteriskVisualizerAction;
    QAction* addMemoryAmpersandVisualizerAction;

    // Enable/disable them depending if the breakpoint already exists.
    if (hasBreakpointLine(lineno) == true) {

        int breakno = breakpointLineToNumber(lineno);

        createAction    = new QAction(QIcon::fromTheme("document-new"), QString("Create breakpoint on line %1").arg(lineno), this);
        deleteAction    = new QAction(QIcon::fromTheme("edit-delete"),  QString("Delete breakpoint %1 on line %2").arg(breakno).arg(lineno), this);
        enableAction    = new QAction(QIcon::fromTheme("list-add"),     QString("Enable breakpoint %1 on line %2").arg(breakno).arg(lineno), this);
        disableAction   = new QAction(QIcon::fromTheme("list-remove"),  QString("Disable breakpoint %1 on line %2").arg(breakno).arg(lineno), this);
        runToLineAction = new QAction(QString("Run to line %1").arg(lineno), this);

        createAction->setEnabled(false);
        deleteAction->setEnabled(true);
        enableAction->setEnabled(true);
        disableAction->setEnabled(true);
        runToLineAction->setEnabled(true);

    }else{
        createAction    = new QAction(QIcon::fromTheme("document-new"), QString("Create breakpoint on line %1").arg(lineno), this);
        deleteAction    = new QAction(QIcon::fromTheme("edit-delete"),  QString("Delete breakpoint on line %1").arg(lineno), this);
        enableAction    = new QAction(QIcon::fromTheme("list-add"),     QString("Enable breakpoint on line %1").arg(lineno), this);
        disableAction   = new QAction(QIcon::fromTheme("list-remove"),  QString("Disable breakpoint on line %1").arg(lineno), this);
        runToLineAction = new QAction(QString("Run to line %1").arg(lineno), this);

        createAction->setEnabled(true);
        deleteAction->setEnabled(false);
        enableAction->setEnabled(false);
        disableAction->setEnabled(false);
        runToLineAction->setEnabled(true);
    }

    addVariableLoggerExpressionAction                   = new QAction(QString("\"%1\"").arg(textCursor().selectedText()));
    addVariableLoggerAsteriskExpressionAction           = new QAction(QString("\"*%1\"").arg(textCursor().selectedText()));
    addVariableLoggerAmpersandExpressionAction          = new QAction(QString("\"&&%1\"").arg(textCursor().selectedText()));
    addVariableLoggerAsteriskAmpersandExpressionAction  = new QAction(QString("\"*&&%1\"").arg(textCursor().selectedText()));
    addVariableTrackerExpressionAction                  = new QAction(QString("\"%1\"").arg(textCursor().selectedText()));
    addVariableTrackerAsteriskExpressionAction          = new QAction(QString("\"*%1\"").arg(textCursor().selectedText()));
    addVariableTrackerAmpersandExpressionAction         = new QAction(QString("\"&&%1\"").arg(textCursor().selectedText()));
    addVariableTrackerAsteriskAmpersandExpressionAction = new QAction(QString("\"*&&%1\"").arg(textCursor().selectedText()));
    addMemoryVisualizerAction                           = new QAction(QString("\"%1\"").arg(textCursor().selectedText()));
    addMemoryAsteriskVisualizerAction                   = new QAction(QString("\"*%1\"").arg(textCursor().selectedText()));
    addMemoryAmpersandVisualizerAction                  = new QAction(QString("\"&&%1\"").arg(textCursor().selectedText()));

    QMenu menu("Breakpoints", this);
    menu.setTitle("Breakpoints");
    menu.addAction(createAction);
    menu.addAction(deleteAction);
    menu.addAction(enableAction);
    menu.addAction(disableAction);
    menu.addAction(runToLineAction);

    QMenu loggerMenu("Add variable to Logger");
    loggerMenu.addAction(addVariableLoggerExpressionAction);
    loggerMenu.addAction(addVariableLoggerAsteriskExpressionAction);
    loggerMenu.addAction(addVariableLoggerAmpersandExpressionAction);
    loggerMenu.addAction(addVariableLoggerAsteriskAmpersandExpressionAction);
    menu.addMenu(&loggerMenu);

    QMenu trackerMenu("Add variable to Tracker");
    trackerMenu.addAction(addVariableTrackerExpressionAction);
    trackerMenu.addAction(addVariableTrackerAsteriskExpressionAction);
    trackerMenu.addAction(addVariableTrackerAmpersandExpressionAction);
    trackerMenu.addAction(addVariableTrackerAsteriskAmpersandExpressionAction);
    menu.addMenu(&trackerMenu);

    QMenu memoryVisualizerMenu("Add variable to a Memory Visualizer");
    memoryVisualizerMenu.addAction(addMemoryVisualizerAction);
    memoryVisualizerMenu.addAction(addMemoryAsteriskVisualizerAction);
    memoryVisualizerMenu.addAction(addMemoryAmpersandVisualizerAction);
    menu.addMenu(&memoryVisualizerMenu);

    // Enable/disable items based on something being selected or not.
    if (textCursor().selectedText() == "") {
        addVariableLoggerExpressionAction->setEnabled(false);
        addVariableLoggerAsteriskExpressionAction->setEnabled(false);
        addVariableLoggerAmpersandExpressionAction->setEnabled(false);
        addVariableLoggerAsteriskAmpersandExpressionAction->setEnabled(false);
        addVariableTrackerExpressionAction->setEnabled(false);
        addVariableTrackerAsteriskExpressionAction->setEnabled(false);
        addVariableTrackerAmpersandExpressionAction->setEnabled(false);
        addVariableTrackerAsteriskAmpersandExpressionAction->setEnabled(false);
        addMemoryVisualizerAction->setEnabled(false);
        addMemoryAsteriskVisualizerAction->setEnabled(false);
        addMemoryAmpersandVisualizerAction->setEnabled(false);
    }else{
        addVariableLoggerExpressionAction->setEnabled(true);
        addVariableLoggerAsteriskExpressionAction->setEnabled(true);
        addVariableLoggerAmpersandExpressionAction->setEnabled(true);
        addVariableLoggerAsteriskAmpersandExpressionAction->setEnabled(true);
        addVariableTrackerExpressionAction->setEnabled(true);
        addVariableTrackerAsteriskExpressionAction->setEnabled(true);
        addVariableTrackerAmpersandExpressionAction->setEnabled(true);
        addVariableTrackerAsteriskAmpersandExpressionAction->setEnabled(true);
        addMemoryVisualizerAction->setEnabled(true);
        addMemoryAsteriskVisualizerAction->setEnabled(true);
        addMemoryAmpersandVisualizerAction->setEnabled(true);
    }

    // Launch the menu. Get the response.
    QAction* action = menu.exec(globalPos);

    // Do nothing.
    if (action == 0) {
        return;
    }

    // Handle creating a new breakpoint.
    if (action == createAction) {

        SeerBreakpointCreateDialog dlg(this);
        dlg.setFilename(fullname());
        dlg.setLineNumber(QString("%1").arg(lineno));

        int ret = dlg.exec();

        if (ret == 0) {
            return;
        }

        //qDebug() << dlg.breakpointText();

        // Emit the create breakpoint signal.
        emit insertBreakpoint(dlg.breakpointText());

        return;
    }

    // Handle deleting a breakpoint.
    if (action == deleteAction) {

        //qDebug() << "deleteBreakpoints" << lineno;

        // Emit the delete breakpoint signal.
        emit deleteBreakpoints(QString("%1").arg(breakpointLineToNumber(lineno)));

        return;
    }

    // Handle enabling a breakpoint.
    if (action == enableAction) {

        //qDebug() << "enableBreakpoints" << lineno;

        // Emit the enable breakpoint signal.
        emit enableBreakpoints(QString("%1").arg(breakpointLineToNumber(lineno)));

        return;
    }

    // Handle disabling a breakpoint.
    if (action == disableAction) {

        //qDebug() << "disableBreakpoints" << lineno;

        // Emit the disable breakpoint signal.
        emit disableBreakpoints(QString("%1").arg(breakpointLineToNumber(lineno)));

        return;
    }

    // Handle running to a line number.
    if (action == runToLineAction) {

        //qDebug() << "runToLine" << lineno;

        // Emit the runToLine signal.
        emit runToLine(fullname(), lineno);

        return;
    }

    // Handle adding a variable to log.
    if (action == addVariableLoggerExpressionAction) {

        //qDebug() << "addVariableLoggerExpression" << lineno;

        // Emit the signals.
        if (textCursor().selectedText() != "") {
            emit addVariableLoggerExpression(textCursor().selectedText());
        }

        return;
    }

    // Handle adding a variable to log.
    if (action == addVariableLoggerAsteriskExpressionAction) {

        //qDebug() << "addVariableLoggerAsteriskExpression" << lineno;

        // Emit the signals.
        if (textCursor().selectedText() != "") {
            emit addVariableLoggerExpression(QString("*") + textCursor().selectedText());
        }

        return;
    }

    // Handle adding a variable to log.
    if (action == addVariableLoggerAmpersandExpressionAction) {

        //qDebug() << "addVariableLoggerAmpersandExpression" << lineno;

        // Emit the signals.
        if (textCursor().selectedText() != "") {
            emit addVariableLoggerExpression(QString("&") + textCursor().selectedText());
        }

        return;
    }

    // Handle adding a variable to log.
    if (action == addVariableLoggerAsteriskAmpersandExpressionAction) {

        //qDebug() << "addVariableLoggerAsteriskAmpersandExpression" << lineno;

        // Emit the signals.
        if (textCursor().selectedText() != "") {
            emit addVariableLoggerExpression(QString("*&") + textCursor().selectedText());
        }

        return;
    }

    // Handle adding a variable to track.
    if (action == addVariableTrackerExpressionAction) {

        //qDebug() << "addVariableTrackerExpression" << lineno;

        // Emit the signals.
        if (textCursor().selectedText() != "") {
            emit addVariableTrackerExpression(textCursor().selectedText());
            emit refreshVariableTrackerValues();
        }

        return;
    }

    // Handle adding a variable to track.
    if (action == addVariableTrackerAsteriskExpressionAction) {

        //qDebug() << "addVariableTrackerAsteriskExpression" << lineno;

        // Emit the signals.
        if (textCursor().selectedText() != "") {
            emit addVariableTrackerExpression(QString("*") + textCursor().selectedText());
            emit refreshVariableTrackerValues();
        }

        return;
    }

    // Handle adding a variable to track.
    if (action == addVariableTrackerAmpersandExpressionAction) {

        //qDebug() << "addVariableTrackerAmpersandExpression" << lineno;

        // Emit the signals.
        if (textCursor().selectedText() != "") {
            emit addVariableTrackerExpression(QString("&") + textCursor().selectedText());
            emit refreshVariableTrackerValues();
        }

        return;
    }

    // Handle adding a variable to track.
    if (action == addVariableTrackerAsteriskAmpersandExpressionAction) {

        //qDebug() << "addVariableTrackerAsteriskAmpersandExpression" << lineno;

        // Emit the signals.
        if (textCursor().selectedText() != "") {
            emit addVariableTrackerExpression(QString("*&") + textCursor().selectedText());
            emit refreshVariableTrackerValues();
        }

        return;
    }

    // Handle adding memory to visualize.
    if (action == addMemoryVisualizerAction) {

        //qDebug() << "addMemoryVisualizer" << lineno;

        // Emit the signals.
        if (textCursor().selectedText() != "") {
            emit addMemoryVisualize(textCursor().selectedText());
        }

        return;
    }

    // Handle adding memory to visualize.
    if (action == addMemoryAsteriskVisualizerAction) {

        //qDebug() << "addMemoryAsteriskVisualizer" << lineno;

        // Emit the signals.
        if (textCursor().selectedText() != "") {
            emit addMemoryVisualize(QString("*") + textCursor().selectedText());
        }

        return;
    }

    // Handle adding memory to visualize.
    if (action == addMemoryAmpersandVisualizerAction) {

        //qDebug() << "addMemoryAmpersandVisualizer" << lineno;

        // Emit the signals.
        if (textCursor().selectedText() != "") {
            emit addMemoryVisualize(QString("&") + textCursor().selectedText());
        }

        return;
    }
}

void SeerEditorWidgetSourceArea::setQuickBreakpoint (QMouseEvent* event) {

    // Get the line number for the cursor position.
    QTextCursor cursor = cursorForPosition(event->pos());

    int lineno = cursor.blockNumber()+1;

    // If there is a breakpoint on the line, toggle it.
    if (hasBreakpointLine(lineno)) {

        // Toggle the breakpoint.
        // Enable if disabled. Disable if enabled.
        if (breakpointLineEnabled(lineno) == false) {
            // Emit the enable breakpoint signal.
            emit enableBreakpoints(QString("%1").arg(breakpointLineToNumber(lineno)));
        }else{
            // Emit the disable breakpoint signal.
            emit disableBreakpoints(QString("%1").arg(breakpointLineToNumber(lineno)));
        }

    // Otherwise, do a quick create of a new breakpoint.
    }else{
        emit insertBreakpoint(QString("-f --source %1 --line %2").arg(fullname()).arg(lineno));
    }
}

void SeerEditorWidgetSourceArea::clearExpression() {

    _selectedExpressionId     = 0;
    _selectedExpressionCursor = QTextCursor();
    _selectedExpressionName   = "";
    _selectedExpressionValue  = "";
}

void SeerEditorWidgetSourceArea::setHighlighterSettings (const SeerHighlighterSettings& settings) {

    _sourceHighlighterSettings = settings;

    emit highlighterSettingsChanged();
}

const SeerHighlighterSettings& SeerEditorWidgetSourceArea::highlighterSettings () const {

    return _sourceHighlighterSettings;
}

void SeerEditorWidgetSourceArea::setHighlighterEnabled (bool flag) {

    _sourceHighlighterEnabled = flag;

    emit highlighterSettingsChanged();
}

bool SeerEditorWidgetSourceArea::highlighterEnabled () const {

    return _sourceHighlighterEnabled;
}

void SeerEditorWidgetSourceArea::handleText (const QString& text) {

    if (text.startsWith("*stopped,")) {

        // *stopped,
        //
        // reason="end-stepping-range",
        //
        // frame={addr="0x0000000000400b45",
        //        func="main",
        //        args=[{name="argc",value="1"},{name="argv",value="0x7fffffffd5b8"}],
        //        file="helloworld.cpp",
        //        fullname="/home/erniep/Development/Peak/src/Seer/helloworld/helloworld.cpp",
        //        line="7",
        //        arch="i386:x86-64"},
        //
        // thread-id="1",
        // stopped-threads="all",
        // core="6"

        QString newtext = Seer::filterEscapes(text); // Filter escaped characters.

        QString frame_text = Seer::parseFirst(newtext, "frame=", '{', '}', false);

        if (frame_text == "") {
            return;
        }

        QString fullname_text = Seer::parseFirst(frame_text, "fullname=", '"', '"', false);
        QString file_text     = Seer::parseFirst(frame_text, "file=",     '"', '"', false);
        QString line_text     = Seer::parseFirst(frame_text, "line=",     '"', '"', false);

        //qDebug() << frame_text;
        //qDebug() << fullname_text << file_text << line_text;

        // Read the file if it hasn't been read before or if we are reading a different file.
        if (fullname_text != fullname()) {
            open(fullname_text, file_text);
        }

        // Set to the line number.
        setCurrentLine(line_text.toInt());

        return;

    }else if (text.contains(QRegExp("^([0-9]+)\\^done,value="))) {

        // 10^done,value="1"
        // 11^done,value="0x7fffffffd538"

        QString id_text = text.section('^', 0,0);

        if (id_text.toInt() == _selectedExpressionId) {

            _selectedExpressionValue = Seer::filterEscapes(Seer::parseFirst(text, "value=", '"', '"', false));

            //qDebug() << _selectedExpressionValue;
        }

        return;

    }else if (text.contains(QRegExp("^([0-9]+)\\^error,msg="))) {

        // 12^error,msg="No symbol \"return\" in current context."
        // 13^error,msg="No symbol \"cout\" in current context."

        QString id_text = text.section('^', 0,0);

        if (id_text.toInt() == _selectedExpressionId) {

            _selectedExpressionValue = Seer::filterEscapes(Seer::parseFirst(text, "msg=", '"', '"', false));

            //qDebug() << _selectedExpressionValue;
        }

        return;

    }else{
        // Ignore others.
        return;
    }

    qDebug() << text;
}

void SeerEditorWidgetSourceArea::handleHighlighterSettingsChanged () {

    // Set base color for background and text color.
    // Use the palette to do this. Some people say to use the stylesheet.
    // But the palettle method works (for now).
    QTextCharFormat format = highlighterSettings().get("Text");

    // qDebug() << QString("QPlainTextEdit {background-color: %1;}").arg(format.background().color().name());
    // #cecece
    // setStyleSheet( QString("QPlainTextEdit {background-color: %1;}").arg(format.background().color().name()) );
    // setStyleSheet( QString("QPlainTextEdit {foreground-color: %1;}").arg(format.foreground().color().name()) );

    QPalette p = palette();
    p.setColor(QPalette::Base, format.background().color());
    p.setColor(QPalette::Text, format.foreground().color());
    setPalette(p);

    // Update the syntax highlighter.
    if (_sourceHighlighter) {

        if (highlighterEnabled()) {
            _sourceHighlighter->setDocument(document());
        }else{
            _sourceHighlighter->setDocument(0);
        }

        _sourceHighlighter->setHighlighterSettings(highlighterSettings());
        _sourceHighlighter->rehighlight();
    }

    // Note. The margins are automatically updated by their own paint events.
    //       The new highlighter settings will be used.
}

