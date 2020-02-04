// enve - 2D animations software
// Copyright (C) 2016-2020 Maurycy Liebner

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef EXPRESSIONEDITOR_H
#define EXPRESSIONEDITOR_H

#include <QTextEdit>
#include <QCompleter>

#include <QStringListModel>
#include <QAbstractItemView>

#include "GUI/global.h"
#include "Animators/qrealanimator.h"

class ExpressionHighlighter;

class ExpressionEditor : public QTextEdit {
public:
    ExpressionEditor(QrealAnimator* const target,
                     QWidget* const parent);

    ExpressionEditor(QrealAnimator* const target,
                     const QString& text, QWidget* const parent);

    void setCompleterList(const QStringList& values);
protected:
    void keyPressEvent(QKeyEvent *e) override;
private:
    void updateVariables(const int from,
                         const int charsRemoved,
                         const int charsAdded);
    void showCompleter();
    void insertCompletion(const QString &completion);
    QString textUnderCursor() const;

    ExpressionHighlighter* mHighlighter;
    QCompleter* mCompleter;

    QRegularExpression mVariableDefinition;
    std::map<int, QStringList> mVariables;
};

#endif // EXPRESSIONEDITOR_H
