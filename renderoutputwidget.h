#ifndef RENDEROUTPUTWIDGET_H
#define RENDEROUTPUTWIDGET_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QComboBox>
#include <QSpinBox>

class RenderOutputWidget : public QDialog {
    Q_OBJECT
public:
    explicit RenderOutputWidget(const qreal &canvasWidth,
                                const qreal &canvasHeight,
                                QWidget *parent = 0);

private:
    qreal mCurrentResolutionFrac = 1.;
    qreal getCurrentResolution();

    qreal mCanvasWidth;
    qreal mCanvasHeight;

    QHBoxLayout *mPathLayout;
    QLabel *mPathLabel;
    QPushButton *mSelectPathButton;

    QVBoxLayout *mMainLayout;

    QPushButton *mRenderButton;

    QHBoxLayout *mSizeLayout;
    QLabel *mWidthLabel;
    QSpinBox *mWidthSpinBox;
    QLabel *mHeightLabel;
    QSpinBox *mHeightSpinBox;

    QLabel *mResoultionLabel;
    QComboBox *mResolutionComboBox;

    void connectSignals();
    void disconnectSignals();
signals:
    void renderOutput(QString, qreal);
public slots:
    void emitRender();
    void chooseDir();
private slots:
    void updateSizeBoxesFromResolution();
    void updateSizeBoxesFromHeight();
    void updateSizeBoxesFromWidth();
};

#endif // RENDEROUTPUTWIDGET_H
