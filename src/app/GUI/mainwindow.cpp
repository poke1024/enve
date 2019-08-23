﻿#include "mainwindow.h"
#include "canvas.h"
#include <QKeyEvent>
#include <QApplication>
#include <QDebug>
#include <QStatusBar>
#include "usagewidget.h"
#include <QToolBar>
#include "GUI/ColorWidgets/colorsettingswidget.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include "boxeslistanimationdockwidget.h"
#include "Tasks/taskexecutor.h"
#include "qdoubleslider.h"
#include "svgimporter.h"
#include "canvaswindow.h"
#include "GUI/BoxesList/boxscrollwidget.h"
#include "clipboardcontainer.h"
#include "GUI/BoxesList/OptimalScrollArea/scrollarea.h"
#include "GUI/BoxesList/boxscrollwidgetvisiblepart.h"
#include "GUI/RenderWidgets/renderwidget.h"
#include "actionbutton.h"
#include "fontswidget.h"
#include "GUI/global.h"
#include "filesourcescache.h"
#include "filesourcelist.h"
#include "videoencoder.h"
#include "fillstrokesettings.h"
#include <QAudioOutput>
#include "Sound/soundcomposition.h"
#include "Sound/singlesound.h"
#include "GUI/BoxesList/boxsinglewidget.h"
#include "memoryhandler.h"
#include "GUI/BrushWidgets/brushselectionwidget.h"
#include "Animators/gradient.h"
#include "canvaswindowwrapper.h"
#include "scenelayout.h"
#include "GUI/GradientWidgets/gradientwidget.h"
#include "GUI/newcanvasdialog.h"
#include "ShaderEffects/shadereffectprogram.h"
#include "importhandler.h"

MainWindow *MainWindow::sInstance = nullptr;

void MainWindow::keyPressEvent(QKeyEvent *event) {
    processKeyEvent(event);
}

class evImporter : public eImporter {
public:
    bool supports(const QFileInfo& fileInfo) const {
        return fileInfo.suffix() == "ev";
    }

    qsptr<BoundingBox> import(const QFileInfo& fileInfo) const {
        MainWindow::sGetInstance()->loadEVFile(fileInfo.absoluteFilePath());
        return nullptr;
    }
};

class eSvgImporter : public eImporter {
public:
    bool supports(const QFileInfo& fileInfo) const {
        return fileInfo.suffix() == "svg";
    }

    qsptr<BoundingBox> import(const QFileInfo& fileInfo) const {
        return loadSVGFile(fileInfo.absoluteFilePath());
    }
};

MainWindow::MainWindow(Document& document,
                       Actions& actions,
                       AudioHandler& audioHandler,
                       RenderHandler& renderHandler,
                       QWidget * const parent)
    : QMainWindow(parent),
      mDocument(document),
      mActions(actions),
      mAudioHandler(audioHandler),
      mRenderHandler(renderHandler) {
    Q_ASSERT(!sInstance);
    sInstance = this;

    ImportHandler::sInstance->addImporter<evImporter>();
    ImportHandler::sInstance->addImporter<eSvgImporter>();

    connect(&mDocument, &Document::evFilePathChanged,
            this, &MainWindow::updateTitle);
    connect(&mDocument, &Document::documentChanged,
            this, [this]() {
        setFileChangedSinceSaving(true);
    });
    connect(&mDocument, &Document::activeSceneSet,
            this, &MainWindow::updateSettingsForCurrentCanvas);
    connect(&mDocument, &Document::currentBoxChanged,
            this, &MainWindow::setCurrentBox);
    connect(&mDocument, &Document::canvasModeSet,
            this, &MainWindow::updateCanvasModeButtonsChecked);
    connect(&mDocument, &Document::sceneCreated,
            this, &MainWindow::closeWelcomeDialog);

    const auto iconDir = eSettings::sIconsDir();
    const auto downArr = iconDir + "/down-arrow.png";
    const auto upArr = iconDir + "/up-arrow.png";
    const QString iconSS =
            "QComboBox::down-arrow { image: url(" + downArr + "); }" +
            "QScrollBar::sub-line { image: url(" + upArr + "); }" +
            "QScrollBar::add-line { image: url(" + downArr + "); }";

    QFile customSS(eSettings::sSettingsDir() + "/stylesheet.qss");
    if(customSS.exists()) {
        if(customSS.open(QIODevice::ReadOnly | QIODevice::Text)) {
            setStyleSheet(customSS.readAll());
            customSS.close();
        }
    } else {
        QFile file(":/styles/stylesheet.qss");
        if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            setStyleSheet(file.readAll() + iconSS);
            file.close();
        }
    }
    BoxSingleWidget::loadStaticPixmaps();

//    for(int i = 0; i < ClipboardContainerType::CCT_COUNT; i++) {
//        mClipboardContainers << nullptr;
//    }

    mDocument.setPath("");

    mFillStrokeSettingsDock = new QDockWidget("Fill and Stroke", this);
    //const auto fillStrokeSettingsScroll = new ScrollArea(this);
    mFillStrokeSettings = new FillStrokeSettingsWidget(mDocument, this);
    //fillStrokeSettingsScroll->setWidget(mFillStrokeSettings);
    const auto fillStrokeDockLabel = new QLabel("Fill and Stroke", this);
    fillStrokeDockLabel->setObjectName("dockLabel");
    fillStrokeDockLabel->setAlignment(Qt::AlignCenter);
    mFillStrokeSettingsDock->setTitleBarWidget(fillStrokeDockLabel);
    mFillStrokeSettingsDock->setWidget(mFillStrokeSettings);
    mFillStrokeSettingsDock->setFeatures(QDockWidget::DockWidgetMovable |
                                         QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, mFillStrokeSettingsDock);
    mFillStrokeSettingsDock->setMinimumWidth(MIN_WIDGET_DIM*12);
    mFillStrokeSettingsDock->setMaximumWidth(MIN_WIDGET_DIM*20);

    mBottomDock = new QDockWidget("Timeline", this);

    mBottomDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    mBottomDock->setTitleBarWidget(new QWidget());
    addDockWidget(Qt::BottomDockWidgetArea, mBottomDock);

    mLayoutHandler = new LayoutHandler(mDocument, mAudioHandler);
    mBoxesListAnimationDockWidget =
            new BoxesListAnimationDockWidget(mDocument, mLayoutHandler, this);
    mBottomDock->setWidget(mBoxesListAnimationDockWidget);

    mBrushSettingsDock = new QDockWidget("Brush Settings", this);
    mBrushSettingsDock->setFeatures(QDockWidget::DockWidgetMovable |
                                    QDockWidget::DockWidgetFloatable);
    mBrushSettingsDock->setMinimumWidth(MIN_WIDGET_DIM*10);
    mBrushSettingsDock->setMaximumWidth(MIN_WIDGET_DIM*20);
    const auto brushDockLabel = new QLabel("Brush Settings", this);
    brushDockLabel->setObjectName("dockLabel");
    brushDockLabel->setAlignment(Qt::AlignCenter);
    mBrushSettingsDock->setTitleBarWidget(brushDockLabel);

    const int ctxt = BrushSelectionWidget::sCreateNewContext();
    mBrushSelectionWidget = new BrushSelectionWidget(ctxt, this);
    connect(mBrushSelectionWidget, &BrushSelectionWidget::currentBrushChanged,
            &mDocument, &Document::setBrush);
//    connect(mBrushSettingsWidget,
//            SIGNAL(brushReplaced(const Brush*,const Brush*)),
//            mCanvasWindow,
//            SLOT(replaceBrush(const Brush*,const Brush*)));

    mBrushSettingsDock->setWidget(mBrushSelectionWidget);

    addDockWidget(Qt::LeftDockWidgetArea, mBrushSettingsDock);
    mBrushSettingsDock->hide();

    mLeftDock = new QDockWidget("Current Object", this);
    mLeftDock->setFeatures(QDockWidget::DockWidgetMovable |
                           QDockWidget::DockWidgetFloatable);
    mLeftDock->setMinimumWidth(MIN_WIDGET_DIM*10);
    mLeftDock->setMaximumWidth(MIN_WIDGET_DIM*20);
    const auto leftDockLabel = new QLabel("Current Object", this);
    leftDockLabel->setObjectName("dockLabel");
    leftDockLabel->setAlignment(Qt::AlignCenter);
    mLeftDock->setTitleBarWidget(leftDockLabel);

    mObjectSettingsScrollArea = new ScrollArea(this);
    mObjectSettingsWidget = new BoxScrollWidget(
                mDocument, mObjectSettingsScrollArea);
    mObjectSettingsScrollArea->setWidget(mObjectSettingsWidget);
    mObjectSettingsWidget->getVisiblePartWidget()->
            setCurrentRule(SWT_BR_SELECTED);
    mObjectSettingsWidget->getVisiblePartWidget()->
            setCurrentTarget(nullptr, SWT_TARGET_CURRENT_GROUP);

    connect(mObjectSettingsScrollArea->verticalScrollBar(),
            &QScrollBar::valueChanged,
            mObjectSettingsWidget, &BoxScrollWidget::changeVisibleTop);
    connect(mObjectSettingsScrollArea, &ScrollArea::heightChanged,
            mObjectSettingsWidget, &BoxScrollWidget::changeVisibleHeight);
    connect(mObjectSettingsScrollArea, &ScrollArea::widthChanged,
            mObjectSettingsWidget, &BoxScrollWidget::setWidth);

    mObjectSettingsScrollArea->verticalScrollBar()->setSingleStep(
                MIN_WIDGET_DIM);

    mLeftDock->setWidget(mObjectSettingsScrollArea);
    addDockWidget(Qt::LeftDockWidgetArea, mLeftDock);

    mLeftDock2 = new QDockWidget(this);
    mLeftDock2->setFeatures(QDockWidget::DockWidgetMovable |
                            QDockWidget::DockWidgetFloatable);
    mLeftDock2->setMinimumWidth(MIN_WIDGET_DIM*10);
    mLeftDock2->setMaximumWidth(MIN_WIDGET_DIM*20);

    mLeftDock2->setWidget(new FileSourceList(this));
    addDockWidget(Qt::LeftDockWidgetArea, mLeftDock2);

    const auto leftDock2Label = new QLabel("Files", this);
    leftDock2Label->setObjectName("dockLabel");
    leftDock2Label->setAlignment(Qt::AlignCenter);
    mLeftDock2->setTitleBarWidget(leftDock2Label);

    setupToolBar();
    setupStatusBar();
    setupMenuBar();

    connectToolBarActions();

    readRecentFiles();
    updateRecentMenu();

    mEventFilterDisabled = false;

    installEventFilter(this);

    openWelcomeDialog();
    showMaximized();
}

MainWindow::~MainWindow() {
//    mtaskExecutorThread->terminate();
//    mtaskExecutorThread->quit();
    BoxSingleWidget::clearStaticPixmaps();
}

#include "noshortcutaction.h"
#include "efiltersettings.h"
void MainWindow::setupMenuBar() {
    mMenuBar = new QMenuBar(this);

    mFileMenu = mMenuBar->addMenu("File");
    mFileMenu->addAction("New...",
                         this, &MainWindow::newFile,
                         Qt::CTRL + Qt::Key_N);
    mFileMenu->addAction("Open...",
                         this, qOverload<>(&MainWindow::openFile),
                         Qt::CTRL + Qt::Key_O);
    mRecentMenu = mFileMenu->addMenu("Open Recent");
    mFileMenu->addSeparator();
    mFileMenu->addAction("Link...",
                         this, &MainWindow::linkFile,
                         Qt::CTRL + Qt::Key_L);
    mFileMenu->addAction("Import File...",
                         this,
                         qOverload<>(&MainWindow::importFile),
                         Qt::CTRL + Qt::Key_I);
    mFileMenu->addAction("Import Image Sequence...",
                         this, &MainWindow::importImageSequence);
    mFileMenu->addSeparator();
    mFileMenu->addAction("Revert", this, &MainWindow::revert);
    mFileMenu->addSeparator();
    mFileMenu->addAction("Save",
                         this, &MainWindow::saveFile,
                         Qt::CTRL + Qt::Key_S);
    mFileMenu->addAction("Save As...",
                         this, &MainWindow::saveFileAs,
                         Qt::CTRL + Qt::SHIFT + Qt::Key_S);
    mFileMenu->addAction("Save Backup",
                         this, &MainWindow::saveBackup);
    mFileMenu->addSeparator();
    mFileMenu->addAction("Close", this, &MainWindow::closeProject);
    mFileMenu->addSeparator();
    mFileMenu->addAction("Exit", this, &MainWindow::close);

    mEditMenu = mMenuBar->addMenu("Edit");

    mEditMenu->addAction("Undo",
                         &mActions, &Actions::undoAction,
                         Qt::CTRL + Qt::Key_Z);
    mEditMenu->addAction("Redo",
                         &mActions, &Actions::redoAction,
                         Qt::CTRL + Qt::SHIFT + Qt::Key_Z);
    mEditMenu->addAction("Undo History...");
    mEditMenu->addSeparator();
    mEditMenu->addAction(new NoShortcutAction("Cut",
                         &mActions, &Actions::cutAction,
                         Qt::CTRL + Qt::Key_X, mEditMenu));
    mEditMenu->addAction(new NoShortcutAction("Copy",
                         &mActions, &Actions::copyAction,
                         Qt::CTRL + Qt::Key_C, mEditMenu));
    mEditMenu->addAction(new NoShortcutAction("Paste",
                         &mActions, &Actions::pasteAction,
                         Qt::CTRL + Qt::Key_V, mEditMenu));
    mEditMenu->addSeparator();
    mEditMenu->addAction(new NoShortcutAction("Duplicate",
                         &mActions, &Actions::duplicateAction,
                         Qt::CTRL + Qt::Key_D, mEditMenu));
    mEditMenu->addSeparator();
    mEditMenu->addAction(new NoShortcutAction("Delete",
                         &mActions, &Actions::deleteAction,
                         Qt::Key_Delete, mEditMenu));
    mEditMenu->addSeparator();
    mEditMenu->addAction(new NoShortcutAction("Select All",
                         &mActions, &Actions::selectAllAction,
                         Qt::Key_A, mEditMenu));
    mEditMenu->addAction(new NoShortcutAction("Invert Selection",
                                              &mActions, &Actions::invertSelectionAction,
                                              Qt::Key_I, mEditMenu));
    mEditMenu->addAction(new NoShortcutAction("Clear Selection",
                         &mActions, &Actions::clearSelectionAction,
                         Qt::ALT + Qt::Key_A, mEditMenu));

//    mSelectSameMenu = mEditMenu->addMenu("Select Same");
//    mSelectSameMenu->addAction("Fill and Stroke");
//    mSelectSameMenu->addAction("Fill Color");
//    mSelectSameMenu->addAction("Stroke Color");
//    mSelectSameMenu->addAction("Stroke Style");
//    mSelectSameMenu->addAction("Object Type");

    mObjectMenu = mMenuBar->addMenu("Object");

    mObjectMenu->addSeparator();
    mObjectMenu->addAction("Raise",
                           &mActions, &Actions::raiseAction,
                           Qt::Key_PageUp);
    mObjectMenu->addAction("Lower",
                           &mActions, &Actions::lowerAction,
                           Qt::Key_PageDown);
    mObjectMenu->addAction("Rasie to Top",
                           &mActions, &Actions::raiseToTopAction,
                           Qt::Key_Home);
    mObjectMenu->addAction("Lower to Bottom",
                           &mActions, &Actions::lowerToBottomAction,
                           Qt::Key_End);
    mObjectMenu->addSeparator();
    mObjectMenu->addAction("Rotate 90° CW",
                           &mActions, &Actions::rotate90CWAction);
    mObjectMenu->addAction("Rotate 90° CCW",
                           &mActions, &Actions::rotate90CCWAction);
    mObjectMenu->addAction("Flip Horizontal", &mActions,
                           &Actions::flipHorizontalAction, Qt::Key_H);
    mObjectMenu->addAction("Flip Vertical", &mActions,
                           &Actions::flipVerticalAction, Qt::Key_V);
    mObjectMenu->addSeparator();
    mObjectMenu->addAction("Group", &mActions,
                           &Actions::groupSelectedBoxes,
                           Qt::CTRL + Qt::Key_G);
    mObjectMenu->addAction("Ungroup", &mActions,
                           &Actions::ungroupSelectedBoxes,
                           Qt::CTRL + Qt::SHIFT + Qt::Key_G);

    mPathMenu = mMenuBar->addMenu("Path");

    mPathMenu->addAction("Object to Path", &mActions,
                         &Actions::objectsToPathAction);
    mPathMenu->addAction("Stroke to Path", &mActions,
                         &Actions::strokeToPathAction);
    mPathMenu->addSeparator();
    mPathMenu->addAction("Union", &mActions,
                         &Actions::pathsUnionAction,
                         Qt::CTRL + Qt::Key_Plus);
    mPathMenu->addAction("Difference", &mActions,
                         &Actions::pathsDifferenceAction,
                         Qt::CTRL + Qt::Key_Minus);
    mPathMenu->addAction("Intersection", &mActions,
                         &Actions::pathsIntersectionAction,
                         Qt::CTRL + Qt::Key_Asterisk);
    mPathMenu->addAction("Exclusion", &mActions,
                         &Actions::pathsExclusionAction,
                         Qt::CTRL + Qt::Key_AsciiCircum);
    mPathMenu->addAction("Division", &mActions,
                         &Actions::pathsDivisionAction,
                         Qt::CTRL + Qt::Key_Slash);
//    mPathMenu->addAction("Cut Path", mCanvas,
//                         &Actions::pathsCutAction);
    mPathMenu->addSeparator();
    mPathMenu->addAction("Combine", &mActions,
                         &Actions::pathsCombineAction,
                         Qt::CTRL + Qt::Key_K);
    mPathMenu->addAction("Break Apart", &mActions,
                         &Actions::pathsBreakApartAction,
                         Qt::CTRL + Qt::SHIFT + Qt::Key_K);

//    mEffectsMenu = mMenuBar->addMenu("Effects");

//    mEffectsMenu->addAction("Blur");

    mSceneMenu = mMenuBar->addMenu("Scene");
    mSceneMenu->addAction("New scene...", this, [this]() {
        CanvasSettingsDialog::sNewCanvasDialog(mDocument, this);
    });

    mViewMenu = mMenuBar->addMenu("View");

    const auto filteringMenu = mViewMenu->addMenu("Filtering");

    mNoneQuality = filteringMenu->addAction("None", [this]() {
        eFilterSettings::sSetDisplayFilter(kNone_SkFilterQuality);
        centralWidget()->update();

        mLowQuality->setChecked(false);
        mMediumQuality->setChecked(false);
        mHighQuality->setChecked(false);
        mDynamicQuality->setChecked(false);
    });
    mNoneQuality->setCheckable(true);
    mNoneQuality->setChecked(eFilterSettings::sDisplay() == kNone_SkFilterQuality &&
                             !eFilterSettings::sSmartDisplat());

    mLowQuality = filteringMenu->addAction("Low", [this]() {
        eFilterSettings::sSetDisplayFilter(kLow_SkFilterQuality);
        centralWidget()->update();

        mNoneQuality->setChecked(false);
        mMediumQuality->setChecked(false);
        mHighQuality->setChecked(false);
        mDynamicQuality->setChecked(false);
    });
    mLowQuality->setCheckable(true);
    mLowQuality->setChecked(eFilterSettings::sDisplay() == kLow_SkFilterQuality &&
                            !eFilterSettings::sSmartDisplat());

    mMediumQuality = filteringMenu->addAction("Medium", [this]() {
        eFilterSettings::sSetDisplayFilter(kMedium_SkFilterQuality);
        centralWidget()->update();

        mNoneQuality->setChecked(false);
        mLowQuality->setChecked(false);
        mHighQuality->setChecked(false);
        mDynamicQuality->setChecked(false);
    });
    mMediumQuality->setCheckable(true);
    mMediumQuality->setChecked(eFilterSettings::sDisplay() == kMedium_SkFilterQuality &&
                               !eFilterSettings::sSmartDisplat());

    mHighQuality = filteringMenu->addAction("High", [this]() {
        eFilterSettings::sSetDisplayFilter(kHigh_SkFilterQuality);
        centralWidget()->update();

        mNoneQuality->setChecked(false);
        mLowQuality->setChecked(false);
        mMediumQuality->setChecked(false);
        mDynamicQuality->setChecked(false);
    });
    mHighQuality->setCheckable(true);
    mHighQuality->setChecked(eFilterSettings::sDisplay() == kHigh_SkFilterQuality &&
                             !eFilterSettings::sSmartDisplat());

    mDynamicQuality = filteringMenu->addAction("Dynamic", [this]() {
        eFilterSettings::sSetSmartDisplay(true);
        centralWidget()->update();

        mLowQuality->setChecked(false);
        mMediumQuality->setChecked(false);
        mHighQuality->setChecked(false);
        mNoneQuality->setChecked(false);
    });
    mDynamicQuality->setCheckable(true);
    mDynamicQuality->setChecked(eFilterSettings::sSmartDisplat());

    mClipViewToCanvas = mViewMenu->addAction("Clip To Canvas");
    mClipViewToCanvas->setCheckable(true);
    mClipViewToCanvas->setChecked(true);
    mClipViewToCanvas->setShortcut(QKeySequence(Qt::Key_C));
    connect(mClipViewToCanvas, &QAction::triggered,
            &mActions, &Actions::setClipToCanvas);

    mRasterEffectsVisible = mViewMenu->addAction("Raster Effects");
    mRasterEffectsVisible->setCheckable(true);
    mRasterEffectsVisible->setChecked(true);
    connect(mRasterEffectsVisible, &QAction::triggered,
            &mActions, &Actions::setRasterEffectsVisible);

    mPathEffectsVisible = mViewMenu->addAction("Path Effects");
    mPathEffectsVisible->setCheckable(true);
    mPathEffectsVisible->setChecked(true);
    connect(mPathEffectsVisible, &QAction::triggered,
            &mActions, &Actions::setPathEffectsVisible);


    mPanelsMenu = mViewMenu->addMenu("Docks");

    mCurrentObjectDock = mPanelsMenu->addAction("Current Object");
    mCurrentObjectDock->setCheckable(true);
    mCurrentObjectDock->setChecked(true);
    mCurrentObjectDock->setShortcut(QKeySequence(Qt::Key_O));

    connect(mCurrentObjectDock, &QAction::toggled,
            mLeftDock, &QDockWidget::setVisible);

    mFilesDock = mPanelsMenu->addAction("Files");
    mFilesDock->setCheckable(true);
    mFilesDock->setChecked(true);
    mFilesDock->setShortcut(QKeySequence(Qt::Key_F));

    connect(mFilesDock, &QAction::toggled,
            mLeftDock2, &QDockWidget::setVisible);

    mObjectsAndAnimationsDock = mPanelsMenu->addAction("Objects and Animations");
    mObjectsAndAnimationsDock->setCheckable(true);
    mObjectsAndAnimationsDock->setChecked(true);
    mObjectsAndAnimationsDock->setShortcut(QKeySequence(Qt::Key_T));

    connect(mObjectsAndAnimationsDock, &QAction::toggled,
            mBottomDock, &QDockWidget::setVisible);

    mFillAndStrokeSettingsDock = mPanelsMenu->addAction("Fill and Stroke");
    mFillAndStrokeSettingsDock->setCheckable(true);
    mFillAndStrokeSettingsDock->setChecked(true);
    mFillAndStrokeSettingsDock->setShortcut(QKeySequence(Qt::Key_E));

    connect(mFillAndStrokeSettingsDock, &QAction::toggled,
            mFillStrokeSettingsDock, &QDockWidget::setVisible);

    mBrushSettingsDockAction = mPanelsMenu->addAction("Brush Settings");
    mBrushSettingsDockAction->setCheckable(true);
    mBrushSettingsDockAction->setChecked(false);
    mBrushSettingsDockAction->setShortcut(QKeySequence(Qt::Key_B));

    connect(mBrushSettingsDockAction, &QAction::toggled,
            mBrushSettingsDock, &QDockWidget::setVisible);

    mRenderMenu = mMenuBar->addMenu("Render");
    mRenderMenu->addAction("Render", this, &MainWindow::addCanvasToRenderQue);

    setMenuBar(mMenuBar);
    mMenuBar->setStyleSheet("QMenuBar { padding-top: 1px; }");
//

}


#include "welcomedialog.h"
void MainWindow::openWelcomeDialog() {
    if(mWelcomeDialog) return;
    mWelcomeDialog = new WelcomeDialog(getRecentFiles(),
       [this]() { CanvasSettingsDialog::sNewCanvasDialog(mDocument, this); },
       []() { MainWindow::sGetInstance()->openFile(); },
       [](QString path) { MainWindow::sGetInstance()->openFile(path); },
       this);
    takeCentralWidget();
    setCentralWidget(mWelcomeDialog);
}

void MainWindow::closeWelcomeDialog() {
    if(!mWelcomeDialog) return;
    mWelcomeDialog = nullptr;
    setCentralWidget(mLayoutHandler->sceneLayout());
}

void MainWindow::addCanvasToRenderQue() {
    if(!mDocument.fActiveScene) return;
    mBoxesListAnimationDockWidget->getRenderWidget()->
    createNewRenderInstanceWidgetForCanvas(mDocument.fActiveScene);
}

void MainWindow::updateSettingsForCurrentCanvas(Canvas* const scene) {
    mObjectSettingsWidget->setCurrentScene(scene);
    if(!scene) {
        mObjectSettingsWidget->setMainTarget(nullptr);
        mBoxesListAnimationDockWidget->updateSettingsForCurrentCanvas(nullptr);
        return;
    }
    mClipViewToCanvas->setChecked(scene->clipToCanvas());
    mRasterEffectsVisible->setChecked(scene->getRasterEffectsVisible());
    mPathEffectsVisible->setChecked(scene->getPathEffectsVisible());
    mBoxesListAnimationDockWidget->updateSettingsForCurrentCanvas(scene);
    mObjectSettingsWidget->setMainTarget(scene->getCurrentGroup());
}

#include <QSpacerItem>
void MainWindow::setupStatusBar() {
    mUsageWidget = new UsageWidget(this);
    mUsageWidget->setStyleSheet("QStatusBar { border-top: 1px solid black; }");
    setStatusBar(mUsageWidget);
}

void MainWindow::setupToolBar() {
    mToolBar = new QToolBar("Toolbar", this);
    mToolBar->setMovable(false);

    mToolBar->setIconSize(QSize(24, 24));

    mToolBar->addSeparator();

    const QString iconsDir = eSettings::sIconsDir() + "/toolbarButtons";

    mBoxTransformMode = new ActionButton(iconsDir + "/boxTransformUnchecked.png", "F1", this);
    mBoxTransformMode->setCheckable(iconsDir + "/boxTransformChecked.png");
    mBoxTransformMode->setChecked(true);
    mToolBar->addWidget(mBoxTransformMode);

    mPointTransformMode = new ActionButton(iconsDir + "/pointTransformUnchecked.png", "F2", this);
    mPointTransformMode->setCheckable(iconsDir + "/pointTransformChecked.png");
    mToolBar->addWidget(mPointTransformMode);

    mAddPointMode = new ActionButton(iconsDir + "/pathCreateUnchecked.png", "F3", this);
    mAddPointMode->setCheckable(iconsDir +  "/pathCreateChecked.png");
    mToolBar->addWidget(mAddPointMode);

    mPaintMode = new ActionButton(iconsDir + "/paintUnchecked.png", "F4", this);
    mPaintMode->setCheckable(iconsDir + "/paintChecked.png");
    mToolBar->addWidget(mPaintMode);

    mToolBar->addSeparator();

    mPickPaintSettingsMode = new ActionButton(iconsDir + "/pickUnchecked.png", "F5", this);
    mPickPaintSettingsMode->setCheckable(iconsDir + "/pickChecked.png");
    mToolBar->addWidget(mPickPaintSettingsMode);

    mCircleMode = new ActionButton(iconsDir + "/circleCreateUnchecked.png", "F6", this);
    mCircleMode->setCheckable(iconsDir + "/circleCreateChecked.png");
    mToolBar->addWidget(mCircleMode);

    mRectangleMode = new ActionButton(iconsDir + "/rectCreateUnchecked.png", "F7", this);
    mRectangleMode->setCheckable(iconsDir + "/rectCreateChecked.png");
    mToolBar->addWidget(mRectangleMode);

    mTextMode = new ActionButton(iconsDir + "/textCreateUnchecked.png", "F8", this);
    mTextMode->setCheckable(iconsDir + "/textCreateChecked.png");
    mToolBar->addWidget(mTextMode);

    mToolBar->addSeparator();

    //mToolBar->addSeparator();
    mToolBar->widgetForAction(mToolBar->addAction("     "))->
            setObjectName("inactiveToolButton");
    //mToolBar->addSeparator();

    mActionConnectPoints = new ActionButton(iconsDir + "/nodeConnect.png",
                                            "CONNECT POINTS", this);
    mToolBar->addWidget(mActionConnectPoints);

    mActionDisconnectPoints = new ActionButton(iconsDir + "/nodeDisconnect.png",
                                               "DISCONNECT POINTS", this);
    mToolBar->addWidget(mActionDisconnectPoints);

    mActionMergePoints = new ActionButton(iconsDir + "/nodeMerge.png",
                                          "MERGE POINTS", this);
    mToolBar->addWidget(mActionMergePoints);
//
    mToolBar->addSeparator();

    mActionSymmetricPointCtrls = new ActionButton(iconsDir + "/nodeSymmetric.png",
                                                  "SYMMETRIC POINTS", this);
    mToolBar->addWidget(mActionSymmetricPointCtrls);

    mActionSmoothPointCtrls = new ActionButton(iconsDir + "/nodeSmooth.png",
                                               "SMOOTH POINTS", this);
    mToolBar->addWidget(mActionSmoothPointCtrls);

    mActionCornerPointCtrls = new ActionButton(iconsDir + "/nodeCorner.png",
                                               "CORNER POINTS", this);
    mToolBar->addWidget(mActionCornerPointCtrls);

//
    mToolBar->addSeparator();

    mActionLine = new ActionButton(iconsDir + "/segmentLine.png",
                                   "MAKE SEGMENT LINE", this);
    mToolBar->addWidget(mActionLine);

    mActionCurve = new ActionButton(iconsDir + "/segmentCurve.png",
                                    "MAKE SEGMENT CURVE", this);
    mToolBar->addWidget(mActionCurve);

    //mToolBar->addSeparator();
    mToolBar->widgetForAction(mToolBar->addAction("     "))->
            setObjectName("inactiveToolButton");
    //mToolBar->addSeparator();
//
    mFontWidget = new FontsWidget(this);
    mToolBar->addWidget(mFontWidget);

    //mToolBar->addSeparator();
    mToolBar->widgetForAction(mToolBar->addAction("     "))->
            setObjectName("inactiveToolButton");
    //mToolBar->addSeparator();

    mToolBar->addWidget(mLayoutHandler->comboWidget());

    addToolBar(mToolBar);

    mDocument.setCanvasMode(CanvasMode::boxTransform);
}

void MainWindow::connectToolBarActions() {
    connect(mBoxTransformMode, &ActionButton::pressed,
            &mActions, &Actions::setMovePathMode);
    connect(mPointTransformMode, &ActionButton::pressed,
            &mActions, &Actions::setMovePointMode);
    connect(mAddPointMode, &ActionButton::pressed,
            &mActions, &Actions::setAddPointMode);
    connect(mPickPaintSettingsMode, &ActionButton::pressed,
            &mActions, &Actions::setPickPaintSettingsMode);
    connect(mCircleMode, &ActionButton::pressed,
            &mActions, &Actions::setCircleMode);
    connect(mRectangleMode, &ActionButton::pressed,
            &mActions, &Actions::setRectangleMode);
    connect(mTextMode, &ActionButton::pressed,
            &mActions, &Actions::setTextMode);
    connect(mPaintMode, &ActionButton::pressed,
            &mActions, &Actions::setPaintMode);
    connect(mActionConnectPoints, &ActionButton::pressed,
            &mActions, &Actions::connectPointsSlot);
    connect(mActionDisconnectPoints, &ActionButton::pressed,
            &mActions, &Actions::disconnectPointsSlot);
    connect(mActionMergePoints, &ActionButton::pressed,
            &mActions, &Actions::mergePointsSlot);
    connect(mActionSymmetricPointCtrls, &ActionButton::pressed,
            &mActions, &Actions::makePointCtrlsSymmetric);
    connect(mActionSmoothPointCtrls, &ActionButton::pressed,
            &mActions, &Actions::makePointCtrlsSmooth);
    connect(mActionCornerPointCtrls, &ActionButton::pressed,
            &mActions, &Actions::makePointCtrlsCorner);
    connect(mActionLine, &ActionButton::pressed,
            &mActions, &Actions::makeSegmentLine);
    connect(mActionCurve, &ActionButton::pressed,
            &mActions, &Actions::makeSegmentCurve);
    connect(mFontWidget, &FontsWidget::fontSizeChanged,
            &mActions, &Actions::setFontSize);
    connect(mFontWidget, &FontsWidget::fontFamilyAndStyleChanged,
            &mActions, &Actions::setFontFamilyAndStyle);
}

MainWindow *MainWindow::sGetInstance() {
    return sInstance;
}

void MainWindow::updateCanvasModeButtonsChecked() {
    const CanvasMode currentMode = mDocument.fCanvasMode;
    mBoxTransformMode->setChecked(currentMode == CanvasMode::boxTransform);
    mPointTransformMode->setChecked(currentMode == CanvasMode::pointTransform);
    mAddPointMode->setChecked(currentMode == CanvasMode::pathCreate);
    mPickPaintSettingsMode->setChecked(currentMode == CanvasMode::pickFillStroke);
    mCircleMode->setChecked(currentMode == CanvasMode::circleCreate);
    mRectangleMode->setChecked(currentMode == CanvasMode::rectCreate);
    mTextMode->setChecked(currentMode == CanvasMode::textCreate);
    mPaintMode->setChecked(currentMode == CanvasMode::paint);
}

//void MainWindow::stopPreview() {
//    mPreviewInterrupted = true;
//    if(!mRendering) {
//        mCurrentRenderFrame = mMaxFrame;
//        mCanvas->clearPreview();
//        mCanvasWindow->repaint();
//        previewFinished();
//    }
//}

void MainWindow::setResolutionFractionValue(const qreal value) {
    if(!mDocument.fActiveScene) return;
    mDocument.fActiveScene->setResolutionFraction(value);
    mDocument.actionFinished();
}

void MainWindow::setFileChangedSinceSaving(const bool changed) {
    if(changed == mChangedSinceSaving) return;
    mChangedSinceSaving = changed;
    updateTitle();
}

SimpleBrushWrapper *MainWindow::getCurrentBrush() const {
    return mBrushSelectionWidget->getCurrentBrush();
}

#include "Boxes/textbox.h"
void MainWindow::setCurrentBox(BoundingBox *box) {
    if(!box) {
        mFillStrokeSettings->setCurrentSettings(nullptr, nullptr);
    } else {
        mFillStrokeSettings->setCurrentSettings(box->getFillSettings(),
                                                box->getStrokeSettings());
        if(box->SWT_isTextBox()) {
            TextBox *txtBox = static_cast<TextBox*>(box);
            mFontWidget->setCurrentSettings(txtBox->getFontSize(),
                                            txtBox->getFontFamily(),
                                            txtBox->getFontStyle());
        }
    }
}

FillStrokeSettingsWidget *MainWindow::getFillStrokeSettings() {
    return mFillStrokeSettings;
}

bool MainWindow::askForSaving() {
    if(mChangedSinceSaving) {
        int buttonId = QMessageBox::question(this, "Save", "Save changes to document \"" +
                                      mDocument.fEvFile.split("/").last() +
                                      "\"?", "Close without saving",
                                      "Cancel",
                                      "Save");
        if(buttonId == 1) {
            return false;
        } else if(buttonId == 2) {
            saveFile();
            return true;
        }
    }
    return true;
}

BoxScrollWidget *MainWindow::getObjectSettingsList() {
    return mObjectSettingsWidget;
}

void MainWindow::disableEventFilter() {
    mEventFilterDisabled = true;
}

void MainWindow::enableEventFilter() {
    mEventFilterDisabled = false;
}

void MainWindow::disable() {
    disableEventFilter();
    mGrayOutWidget = new QWidget(this);
    mGrayOutWidget->setFixedSize(size());
    mGrayOutWidget->setStyleSheet(
                "QWidget { background-color: rgb(0, 0, 0, 125) }");
    mGrayOutWidget->show();
    mGrayOutWidget->update();
}

void MainWindow::enable() {
    if(!mGrayOutWidget) return;
    enableEventFilter();
    delete mGrayOutWidget;
    mGrayOutWidget = nullptr;
    mDocument.actionFinished();
}

void MainWindow::newFile() {
    if(askForSaving()) {
        closeProject();
        CanvasSettingsDialog::sNewCanvasDialog(mDocument, this);
    }
}

bool handleCanvasModeKeyPress(Document& document, const int key) {
    if(key == Qt::Key_F1) {
        document.setCanvasMode(CanvasMode::boxTransform);
    } else if(key == Qt::Key_F2) {
        document.setCanvasMode(CanvasMode::pointTransform);
    } else if(key == Qt::Key_F3) {
        document.setCanvasMode(CanvasMode::pathCreate);
    }  else if(key == Qt::Key_F4) {
        document.setCanvasMode(CanvasMode::paint);
    } else if(key == Qt::Key_F5) {
        document.setCanvasMode(CanvasMode::pickFillStroke);
    } else if(key == Qt::Key_F6) {
        document.setCanvasMode(CanvasMode::circleCreate);
    } else if(key == Qt::Key_F7) {
        document.setCanvasMode(CanvasMode::rectCreate);
    } else if(key == Qt::Key_F8) {
        document.setCanvasMode(CanvasMode::textCreate);
    } else return false;
    KeyFocusTarget::KFT_sSetRandomTarget();
    return true;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e) {
    if(mLock) if(dynamic_cast<QInputEvent*>(e)) return true;
    if(mEventFilterDisabled) return QMainWindow::eventFilter(obj, e);
    const auto type = e->type();
    const auto focusWidget = QApplication::focusWidget();
    if(type == QEvent::KeyPress) {
        const auto keyEvent = static_cast<QKeyEvent*>(e);
        if(keyEvent->key() == Qt::Key_Delete && focusWidget) {
            mEventFilterDisabled = true;
            const bool widHandled =
                    QCoreApplication::sendEvent(focusWidget, keyEvent);
            mEventFilterDisabled = false;
            if(widHandled) return false;
        }
        return processKeyEvent(keyEvent);
    } else if(type == QEvent::ShortcutOverride) {
        const auto keyEvent = static_cast<QKeyEvent*>(e);
        const int key = keyEvent->key();
        if(key == Qt::Key_Tab) {
            KeyFocusTarget::KFT_sTab();
            return true;
        }
        if(handleCanvasModeKeyPress(mDocument, key)) return true;
        if(keyEvent->modifiers() & Qt::SHIFT && key == Qt::Key_D) {
            return processKeyEvent(keyEvent);
        }
        if(keyEvent->modifiers() & Qt::CTRL) {
            if(key == Qt::Key_C || key == Qt::Key_V ||
               key == Qt::Key_X || key == Qt::Key_D) {
                return processKeyEvent(keyEvent);
            }
        } else if(key == Qt::Key_A || key == Qt::Key_I ||
                  key == Qt::Key_Delete) {
              return processKeyEvent(keyEvent);
        }
    } else if(type == QEvent::KeyRelease) {
        const auto keyEvent = static_cast<QKeyEvent*>(e);
        if(processKeyEvent(keyEvent)) return true;
        //finishUndoRedoSet();
    } else if(type == QEvent::MouseButtonRelease) {
        //finishUndoRedoSet();
    }
    return QMainWindow::eventFilter(obj, e);
}

void MainWindow::closeEvent(QCloseEvent *e) {
    if(!closeProject()) e->ignore();
}

bool MainWindow::processKeyEvent(QKeyEvent *event) {
    if(isActiveWindow()) {
        bool returnBool = false;
        if(event->type() == QEvent::KeyPress &&
           mBoxesListAnimationDockWidget->processKeyPress(event)) {
            returnBool = true;
        } else {
            returnBool = KeyFocusTarget::KFT_handleKeyEvent(event);
        }
        mDocument.actionFinished();
        return returnBool;
    }
    return false;
}

bool MainWindow::isEnabled() {
    return !mGrayOutWidget;
}

void MainWindow::clearAll() {
    TaskScheduler::sInstance->clearTasks();
    setFileChangedSinceSaving(false);
    mObjectSettingsWidget->setMainTarget(nullptr);

    mBoxesListAnimationDockWidget->clearAll();
    mFillStrokeSettings->clearAll();
    mDocument.clear();
    mLayoutHandler->clear();
//    for(ClipboardContainer *cont : mClipboardContainers) {
//        delete cont;
//    }
//    mClipboardContainers.clear();
    FilesHandler::sInstance->clear();
    //mBoxListWidget->clearAll();
    openWelcomeDialog();
}

void MainWindow::updateTitle() {
    QString star = "";
    if(mChangedSinceSaving) star = "*";
    setWindowTitle(mDocument.fEvFile.split("/").last() + star + " - enve");
}

void MainWindow::openFile() {
    if(askForSaving()) {
        disable();
        const QString openPath = QFileDialog::getOpenFileName(this,
            "Open File", mDocument.fEvFile, "enve Files (*.ev)");
        if(!openPath.isEmpty()) openFile(openPath);
        enable();
    }
}

void MainWindow::openFile(const QString& openPath) {
    clearAll();
    try {
        loadEVFile(openPath);
        mDocument.setPath(openPath);
        setFileChangedSinceSaving(false);
    } catch(const std::exception& e) {
        gPrintExceptionCritical(e);
    }
}

void MainWindow::saveFile() {
    if(mDocument.fEvFile.isEmpty()) {
        saveFileAs();
        return;
    }
    try {
        saveToFile(mDocument.fEvFile);
        setFileChangedSinceSaving(false);
    } catch(const std::exception& e) {
        gPrintExceptionCritical(e);
    }
}

void MainWindow::saveFileAs() {
    disableEventFilter();
    const QString saveAs = QFileDialog::getSaveFileName(this, "Save File",
                               mDocument.fEvFile,
                               "enve Files (*.ev)");
    enableEventFilter();
    if(!saveAs.isEmpty()) {
        try {
            saveToFile(saveAs);
            mDocument.setPath(saveAs);
            setFileChangedSinceSaving(false);
        } catch(const std::exception& e) {
            gPrintExceptionCritical(e);
        }
    }
}

void MainWindow::saveBackup() {
    const QString backupPath = "backup/backup_%1.ev";
    int id = 1;
    QFile backupFile(backupPath.arg(id));
    while(backupFile.exists()) {
        id++;
        backupFile.setFileName(backupPath.arg(id) );
    }
    try {
        saveToFile(backupPath.arg(id));
    } catch(const std::exception& e) {
        gPrintExceptionCritical(e);
    }
}

bool MainWindow::closeProject() {
    if(askForSaving()) {
        clearAll();
        return true;
    }
    return false;
}

void MainWindow::importFile() {
    disableEventFilter();
    QStringList importPaths = QFileDialog::getOpenFileNames(
                                            this,
                                            "Import File", "",
                                            "Files (*.ev *.svg "
                                                   "*.mp4 *.mov *.avi *.mkv *.m4v "
                                                   "*.png *.jpg "
                                                   "*.wav *.mp3)");
    enableEventFilter();
    if(!importPaths.isEmpty()) {
        for(const QString &path : importPaths) {
            if(path.isEmpty()) continue;
            try {
                mActions.importFile(path);
            } catch(const std::exception& e) {
                gPrintExceptionCritical(e);
            }
        }
    }
}

void MainWindow::linkFile() {
    disableEventFilter();
    QStringList importPaths = QFileDialog::getOpenFileNames(this,
        "Link File", "", "enve Files (*.ev)");
    enableEventFilter();
    if(!importPaths.isEmpty()) {
        for(const QString &path : importPaths) {
            if(path.isEmpty()) continue;
            mDocument.fActiveScene->createLinkToFileWithPath(path);
        }
    }
}

void MainWindow::importImageSequence() {
    disableEventFilter();
    const auto folder = QFileDialog::getExistingDirectory(
                this, "Import Image Sequence", "");
    enableEventFilter();
    if(!folder.isEmpty()) {
        mDocument.fActiveScene->createAnimationBoxForPaths(folder);
    }
    mDocument.actionFinished();
}

//void MainWindow::importVideo() {
//    disableEventFilter();
//    QStringList importPaths = QFileDialog::getOpenFileNames(this,
//        "Import Video", "", "Video (*.mp4 *.mov *.avi)");
//    enableEventFilter();
//    for(const QString &path : importPaths) {
//        mCanvasWindow->createVideoForPath(path);
//    }
//    callUpdateSchedulers();
//}

void MainWindow::revert() {
    clearAll();
    try {
        loadEVFile(mDocument.fEvFile);
    } catch(const std::exception& e) {
        gPrintExceptionCritical(e);
    }
    setFileChangedSinceSaving(false);
}

stdsptr<void> MainWindow::lock() {
    if(mLock) return mLock->ref<Lock>();
    setEnabled(false);
    const auto newLock = enve::make_shared<Lock>(this);
    mLock = newLock.get();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    return newLock;
}

void MainWindow::lockFinished() {
    if(mLock) {
        gPrintException(false, "Lock finished before lock object deleted");
    } else {
        QApplication::restoreOverrideCursor();
        setEnabled(true);
    }
}

void MainWindow::updateRecentMenu() {
    mRecentMenu->clear();
    const auto homePath = QDir::homePath();
    for(const auto& path : mRecentFiles) {
        QString ttPath = path;
        if(ttPath.left(homePath.count()) == homePath) {
            ttPath = "~" + ttPath.mid(homePath.count());
        }
        mRecentMenu->addAction(path, [path, this]() {
            openFile(path);
        });
    }
}
