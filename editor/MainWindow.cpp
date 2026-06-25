#include "MainWindow.h"

#include <QStatusBar>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QUrl>
#include <QFileInfo>
#include <QFontInfo>
#include <QInputDialog>
#include <QDir>
#include <QProcess>
#include <QTabWidget>
#include <QSplitter>
#include <QShortcut>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTimer>
#include <QLineEdit>
#include <QPalette>

#include "Code.h"
#include "Syntax.h"
#include "LspClient.h"
#include "LspCompletionModel.h"
#include "DiagnosticsPanel.h"
#include "CompilerPanel.h"
#include "SnippetManager.h"
#include "BreadcrumbsBar.h"
#include "ActivityBar.h"
#include "FileTreePanel.h"
#include "ContextManager.h"
#include "FindReplaceBar.h"

static QString baseStyle()
{
    return QStringLiteral(
        "QMainWindow, QWidget { background: #11111b; color: #c6d0f5; }"
        "QMenuBar { background: #11111b; color: #c6d0f5; border-bottom: 1px solid #1e1e2e; padding: 2px; }"
        "QMenuBar::item:selected { background: #363a4f; }"
        "QMenuBar::item:pressed { background: #209fb5; }"
        "QMenu { background: #1e1e2e; color: #c6d0f5; border: 1px solid #363a4f; }"
        "QMenu::item:selected { background: #363a4f; }"
        "QMenu::separator { height: 1px; background: #363a4f; margin: 4px 8px; }"
        "QStatusBar { background: #11111b; color: #a5adce; border-top: 1px solid #1e1e2e; font-size: 11px; }"
        "QStatusBar::item { border: none; }"
        "QDockWidget { background: #11111b; color: #c6d0f5; }"
        "QDockWidget::title { background: #1e1e2e; color: #c6d0f5; padding: 4px; }"
    );
}

QIcon createZithIcon()
{
    return QIcon(QStringLiteral(":/icons/zith-icon.svg"));
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_initialContextSetup(true)
{
    setWindowTitle("Zith Studio");
    setWindowIcon(createZithIcon());
    resize(1000, 700);
    setStyleSheet(baseStyle());

    m_tabWidget = new QTabWidget;
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane { border: none; background: #11111b; }"
        "QTabBar { background: #11111b; }"
        "QTabBar::tab { background: #11111b; color: #9ca0b0; padding: 4px 14px;"
        "  border-right: 1px solid #1e1e2e; font-size: 12px; }"
        "QTabBar::tab:selected { color: #c6d0f5; }"
        "QTabBar::tab:hover:!selected { background: #181825; }"
    );

    m_breadcrumbs = new BreadcrumbsBar;
    m_findReplaceBar = new FindReplaceBar;
    m_findReplaceBar->hide();

    QWidget *editorPanel = new QWidget;
    QVBoxLayout *editorLayout = new QVBoxLayout(editorPanel);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    editorLayout->addWidget(m_breadcrumbs);
    editorLayout->addWidget(m_findReplaceBar);
    editorLayout->addWidget(m_tabWidget, 1);

    m_fileTree = new FileTreePanel;
    m_fileTree->setMinimumWidth(150);
    m_fileTree->setMaximumWidth(400);

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->addWidget(m_fileTree);
    m_splitter->addWidget(editorPanel);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setSizes({220, 800});

    setCentralWidget(m_splitter);

    auto applyZoom = [this](double factor) {
        int basePt = 11;
        auto scale = [&](QLabel *label) {
            if (!label) return;
            QFont f = label->font();
            f.setPointSize(qMax(8, int(basePt * factor)));
            label->setFont(f);
        };
        scale(m_contextLabel);
        scale(m_lspLabel);
        scale(m_errorLabel);
        scale(m_posLabel);
        scale(m_indentLabel);
        scale(m_encodingLabel);
        scale(m_langLabel);
    };

    m_snippetManager = new SnippetManager(this);
    m_snippetManager->loadFromJson(
        "/home/diogo/zith-extension/vs-code/snippets/zith.json");

    m_completionModel = new LspCompletionModel(this);
    m_completer = new LspCompleter(m_completionModel, this);

    m_completer->popup()->setStyleSheet(
        "QAbstractItemView { background: #1e1e2e; color: #c6d0f5; "
        "border: 1px solid #363a4f; outline: none; }"
        "QAbstractItemView::item:selected { background: #363a4f; }"
        "QAbstractItemView::item:alternate { background: #181825; }"
    );

    m_lspClient = new LspClient(this);

    QString lspPath = "/home/diogo/zith-lsp/build/zith-lsp";
    QString stdlibPath = "/home/diogo/zith-lsp/build/_deps/zithc-src/stdlib";
    if (!m_lspClient->start(lspPath, stdlibPath)) {
        statusBar()->showMessage("Failed to start LSP server", 5000);
    }

    connect(m_lspClient, &LspClient::completionResults,
            this, [this](const QList<LspCompletionItem> &items) {
        auto all = items;
        all.append(m_snippetManager->allSnippets());
        m_completionModel->setItems(all);
        if (!items.isEmpty())
            m_completer->complete();
    });

    connect(m_lspClient, &LspClient::initialized, this, [this]() {
        statusBar()->showMessage("LSP connected", 3000);
        auto *ed = currentEditor();
        if (ed && !ed->filePath().isEmpty()) {
            QFile f(ed->filePath());
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_lspClient->openDocument(
                    ed->fileUri(), "zith",
                    QString::fromUtf8(f.readAll()));
                f.close();
            }
        }
    });

    connect(m_lspClient, &LspClient::serverError,
            this, [this](const QString &msg) {
        statusBar()->showMessage("LSP: " + msg, 5000);
    });

    m_contextLabel = new QLabel("◀ 1/1 ▶");
    m_contextLabel->setStyleSheet("color: #7287fd; padding: 0 4px;");

    m_lspLabel = new QLabel("LSP ⬤");
    m_lspLabel->setStyleSheet("color: #a6d189; padding: 0 4px;");

    m_errorLabel = new QLabel("✕0  ⚠0");
    m_errorLabel->setStyleSheet("color: #a5adce; padding: 0 4px;");

    m_posLabel = new QLabel("Ln 1, Col 1");
    m_posLabel->setStyleSheet("color: #a5adce; padding: 0 4px;");

    m_indentLabel = new QLabel("Spaces: 4");
    m_indentLabel->setStyleSheet("color: #6c7086; padding: 0 4px;");

    m_encodingLabel = new QLabel("UTF-8");
    m_encodingLabel->setStyleSheet("color: #6c7086; padding: 0 4px;");

    m_langLabel = new QLabel("Zith");
    m_langLabel->setStyleSheet("color: #8839ef; padding: 0 4px; font-weight: bold;");

    statusBar()->addWidget(m_contextLabel);
    statusBar()->addWidget(m_lspLabel);

    statusBar()->addPermanentWidget(m_errorLabel);
    statusBar()->addPermanentWidget(m_posLabel);
    statusBar()->addPermanentWidget(m_indentLabel);
    statusBar()->addPermanentWidget(m_encodingLabel);
    statusBar()->addPermanentWidget(m_langLabel);

    m_contextManager = new ContextManager(this);

    connect(m_fileTree, &FileTreePanel::fileActivated,
            this, [this](const QString &path) {
        openFilePath(path);
    });

    auto *ctxLeft = new QShortcut(QKeySequence("Alt+Left"), this);
    connect(ctxLeft, &QShortcut::activated, this, [this]() {
        saveContextState();
        m_contextManager->navigateLeft();
    });

    auto *ctxRight = new QShortcut(QKeySequence("Alt+Right"), this);
    connect(ctxRight, &QShortcut::activated, this, [this]() {
        int last = m_contextManager->count() - 1;
        int cur = m_contextManager->currentIndex();
        if (cur == last && !m_contextManager->currentRoot().isEmpty()) {
            QString dir = QFileDialog::getExistingDirectory(this, "Open project folder");
            if (!dir.isEmpty()) {
                saveContextState();
                m_contextManager->appendNew(dir);
            }
        } else if (cur < last) {
            saveContextState();
            m_contextManager->navigateRight();
        }
    });

    connect(m_contextManager, &ContextManager::contextChanged,
            this, [this](int index, const Context &ctx) {
        m_fileTree->setRootPath(ctx.rootPath);
        if (m_contextLabel)
            m_contextLabel->setText(QString("%1/%2")
                .arg(index + 1).arg(m_contextManager->count()));
        if (!m_initialContextSetup)
            restoreContextState(ctx);
    });

    QString defaultFile = "/home/diogo/zith-extension/vs-code/example.zith";
    if (QFileInfo::exists(defaultFile)) {
        m_contextManager->setCurrentRoot(QFileInfo(defaultFile).absolutePath());
        openFilePath(defaultFile);
    } else {
        m_contextManager->setCurrentRoot(QDir::homePath());
        createTab();
    }

    auto *findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        m_findReplaceBar->setEditor(currentEditor());
        m_findReplaceBar->showFind();
    });

    auto *replaceShortcut = new QShortcut(QKeySequence("Ctrl+H"), this);
    connect(replaceShortcut, &QShortcut::activated, this, [this]() {
        m_findReplaceBar->setEditor(currentEditor());
        m_findReplaceBar->showReplace();
    });

    auto *findNextShortcut = new QShortcut(QKeySequence::FindNext, this);
    connect(findNextShortcut, &QShortcut::activated, this, [this]() {
        if (m_findReplaceBar->isVisible()) {
            m_findReplaceBar->findNext();
        } else {
            m_findReplaceBar->setEditor(currentEditor());
            m_findReplaceBar->showFind();
        }
    });

    auto *findPrevShortcut = new QShortcut(QKeySequence::FindPrevious, this);
    connect(findPrevShortcut, &QShortcut::activated, this, [this]() {
        if (m_findReplaceBar->isVisible()) {
            m_findReplaceBar->findPrevious();
        } else {
            m_findReplaceBar->setEditor(currentEditor());
            m_findReplaceBar->showFind();
        }
    });

    m_initialContextSetup = false;
    saveContextState();

    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int idx) {
        auto *ed = qobject_cast<CodeEditor*>(m_tabWidget->widget(idx));
        if (!ed) return;
        m_findReplaceBar->setEditor(ed);
        if (ed->filePath().isEmpty())
            m_breadcrumbs->clear();
        else
            m_breadcrumbs->setPath(ed->filePath());
        QTextCursor c = ed->textCursor();
        m_posLabel->setText(QString("Ln %1, Col %2")
            .arg(c.blockNumber() + 1).arg(c.positionInBlock() + 1));
        setWindowTitle("Zith Studio - " + (ed->filePath().isEmpty()
            ? QString("Untitled") : QFileInfo(ed->filePath()).fileName()));
    });

    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, [this](int idx) {
        auto *ed = qobject_cast<CodeEditor*>(m_tabWidget->widget(idx));
        if (!ed) return;
        if (ed->document()->isModified()) {
            auto result = QMessageBox::question(this, "Save?",
                QString("Save changes to '%1'?").arg(
                    ed->filePath().isEmpty() ? "Untitled" : ed->filePath()),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            if (result == QMessageBox::Save) {
                bool was = (ed == currentEditor());
                m_tabWidget->setCurrentIndex(idx);
                saveFile();
                if (!ed->document()->isModified()) {
                    m_tabWidget->removeTab(idx);
                    m_highlighters.remove(ed);
                    ed->deleteLater();
                }
                return;
            } else if (result == QMessageBox::Cancel) {
                return;
            }
        }
        m_tabWidget->removeTab(idx);
        m_highlighters.remove(ed);
        ed->deleteLater();
    });

    m_activityBar = new ActivityBar(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_activityBar);

    connect(m_activityBar, &ActivityBar::modeChanged,
            this, [this](ActivityBar::Mode mode) {
        if (mode == ActivityBar::Explorer) {
            QList<int> sizes = m_splitter->sizes();
            bool visible = sizes[0] > 0;
            if (visible) {
                m_fileTreeWidth = sizes[0];
                toggleFileTree(false);
            } else {
                toggleFileTree(true);
            }
        } else if (mode == ActivityBar::Search) {
            if (m_findReplaceBar->isVisible()) {
                m_findReplaceBar->hide();
            } else {
                m_findReplaceBar->setEditor(currentEditor());
                m_findReplaceBar->showFind();
            }
        }
    });

    m_diagnosticsPanel = new DiagnosticsPanel(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_diagnosticsPanel);

    m_compilerPanel = new CompilerPanel(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_compilerPanel);
    m_compilerPanel->hide();

    connect(m_lspClient, &LspClient::diagnosticsReceived,
            m_diagnosticsPanel, &DiagnosticsPanel::setDiagnostics);

    connect(m_diagnosticsPanel, &DiagnosticsPanel::navigateToLocation,
            this, [this](const QString &uri, int line, int character) {
        auto *ed = currentEditor();
        if (ed && QUrl(uri).toLocalFile() == ed->filePath())
            ed->goToLine(line, character);
    });

    connect(m_diagnosticsPanel, &DiagnosticsPanel::countsChanged,
            this, [this](int errors, int warnings) {
        m_errorLabel->setText(QString("E:%1  W:%2").arg(errors).arg(warnings));
    });

    QMenu *fileMenu = menuBar()->addMenu("&File");

    QAction *newAct = fileMenu->addAction("&New File", QKeySequence::New);
    connect(newAct, &QAction::triggered, this, [this]() { newFile(); });

    QAction *newWinAct = fileMenu->addAction("New &Window", QKeySequence("Ctrl+Shift+N"));
    connect(newWinAct, &QAction::triggered, this, [this]() {
        auto *win = new MainWindow;
        win->setAttribute(Qt::WA_DeleteOnClose);
        win->show();
    });

    QAction *newProjAct = fileMenu->addAction("New &Project...", QKeySequence("Ctrl+Alt+N"));
    connect(newProjAct, &QAction::triggered, this, [this]() { newProject(); });

    QAction *openAct = fileMenu->addAction("&Open...", QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, [this]() { openFile(); });

    QAction *saveAct = fileMenu->addAction("&Save", QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, [this]() { saveFile(); });

    fileMenu->addSeparator();

    QAction *exitAct = fileMenu->addAction("E&xit", QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, [this]() { close(); });

    QMenu *toolsMenu = menuBar()->addMenu("&Tools");

    auto runZithc = [this](const QString &cmd, bool needsFile) {
        if (QProcess::execute("zithc", {"--version"}) != 0) {
            statusBar()->showMessage("zithc not found in PATH", 5000);
            return;
        }
        auto *ed = currentEditor();
        if (!ed) return;
        if (needsFile && ed->filePath().isEmpty()) {
            statusBar()->showMessage("No file open", 5000);
            return;
        }
        QStringList args = {cmd};
        if (needsFile)
            args << ed->filePath();
        QString dir = ed->filePath().isEmpty()
            ? QDir::homePath()
            : QFileInfo(ed->filePath()).absolutePath();
        m_compilerPanel->show();
        m_compilerPanel->raise();
        m_compilerPanel->runCommand(dir, "zithc", args);
    };

    QAction *buildAct = toolsMenu->addAction("&Build", QKeySequence("Ctrl+B"));
    connect(buildAct, &QAction::triggered, this, [runZithc]() { runZithc("build", false); });

    QAction *checkAct = toolsMenu->addAction("&Check File", QKeySequence("Ctrl+Shift+C"));
    connect(checkAct, &QAction::triggered, this, [runZithc]() { runZithc("check", true); });

    QAction *compileAct = toolsMenu->addAction("&Compile File", QKeySequence("Ctrl+Shift+B"));
    connect(compileAct, &QAction::triggered, this, [runZithc]() { runZithc("compile", true); });

    QAction *runAct = toolsMenu->addAction("&Run", QKeySequence("Ctrl+Shift+R"));
    connect(runAct, &QAction::triggered, this, [runZithc]() { runZithc("run", true); });

    toolsMenu->addSeparator();

    QAction *restartLspAct = toolsMenu->addAction("Restart &LSP", QKeySequence("Ctrl+Shift+L"));
    connect(restartLspAct, &QAction::triggered, this, [this]() {
        m_lspClient->stop();
        m_completionModel->setItems({});
        QString lspPath = "/home/diogo/zith-lsp/build/zith-lsp";
        QString stdlibPath = "/home/diogo/zith-lsp/build/_deps/zithc-src/stdlib";
        if (m_lspClient->start(lspPath, stdlibPath)) {
            statusBar()->showMessage("LSP restarting...", 3000);
        } else {
            statusBar()->showMessage("Failed to restart LSP", 5000);
        }
    });

    QMenu *viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(m_diagnosticsPanel->toggleViewAction());
    viewMenu->addAction(m_compilerPanel->toggleViewAction());
}

void MainWindow::openFilePath(const QString &path)
{
    if (path.isEmpty()) return;

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *ed = qobject_cast<CodeEditor*>(m_tabWidget->widget(i));
        if (ed && ed->filePath() == path) {
            m_tabWidget->setCurrentIndex(i);
            m_breadcrumbs->setPath(path);
            return;
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    auto *editor = createTab();
    editor->setFilePath(path);
    editor->setPlainText(content);
    editor->document()->setModified(false);
    editor->clearDiagnostics();

    int idx = m_tabWidget->currentIndex();
    m_tabWidget->setTabText(idx, QFileInfo(path).fileName());
    m_tabWidget->setTabToolTip(idx, path);

    m_breadcrumbs->setPath(path);
    if (m_diagnosticsPanel) m_diagnosticsPanel->clear();
    if (m_completionModel) m_completionModel->setItems({});

    setWindowTitle("Zith Studio - " + QFileInfo(path).fileName());

    if (m_lspClient && m_lspClient->isRunning())
        m_lspClient->openDocument(editor->fileUri(), "zith", content);
}

CodeEditor *MainWindow::createTab()
{
    auto *editor = new CodeEditor;

    QFont font("JetBrains Mono", 12);
    if (!QFontInfo(font).exactMatch())
        font = QFont("Monospace", 12);
    font.setStyleStrategy(QFont::PreferAntialias);
    editor->setFont(font);

    auto *hl = new SyntaxHighlighter(editor->document());
    m_highlighters.insert(editor, hl);

    editor->setSnippetManager(m_snippetManager);
    editor->setCompleter(m_completer);
    editor->setLspClient(m_lspClient);

    connectEditorSignals(editor);

    int idx = m_tabWidget->addTab(editor, "Untitled");
    m_tabWidget->setCurrentIndex(idx);
    m_tabWidget->setTabToolTip(idx, {});

    return editor;
}

void MainWindow::connectEditorSignals(CodeEditor *editor)
{
    connect(editor, &CodeEditor::cursorPositionChanged, this, [this, editor]() {
        if (editor != currentEditor()) return;
        QTextCursor c = editor->textCursor();
        m_posLabel->setText(QString("Ln %1, Col %2")
            .arg(c.blockNumber() + 1).arg(c.positionInBlock() + 1));
    });

    connect(editor, &CodeEditor::zoomChanged, this, [this](double factor) {
        int basePt = 11;
        auto scale = [&](QLabel *label) {
            QFont f = label->font();
            f.setPointSize(qMax(8, int(basePt * factor)));
            label->setFont(f);
        };
        scale(m_contextLabel);
        scale(m_lspLabel);
        scale(m_errorLabel);
        scale(m_posLabel);
        scale(m_indentLabel);
        scale(m_encodingLabel);
        scale(m_langLabel);
    });

    connect(editor, &CodeEditor::navigateToLocation,
            this, [this](const QString &uri, int line, int character) {
        QString localPath = QUrl(uri).toLocalFile();
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            auto *ed = qobject_cast<CodeEditor*>(m_tabWidget->widget(i));
            if (ed && ed->filePath() == localPath) {
                m_tabWidget->setCurrentIndex(i);
                ed->goToLine(line, character);
                return;
            }
        }
        openFilePath(localPath);
        auto *ed = currentEditor();
        if (ed && ed->filePath() == localPath)
            ed->goToLine(line, character);
    });

    connect(m_lspClient, &LspClient::diagnosticsReceived,
            editor, [editor](const QString &uri, const QList<LspDiagnostic> &diags) {
        if (uri == editor->fileUri())
            editor->setDiagnostics(diags);
    });

    connect(editor->document(), &QTextDocument::modificationChanged,
            this, [this, editor](bool changed) {
        int idx = m_tabWidget->indexOf(editor);
        if (idx < 0) return;
        QString text = m_tabWidget->tabText(idx);
        if (changed && !text.endsWith(" ●"))
            m_tabWidget->setTabText(idx, text + " ●");
        else if (!changed && text.endsWith(" ●"))
            m_tabWidget->setTabText(idx, text.chopped(2));
    });
}

void MainWindow::newFile()
{
    createTab();
    m_diagnosticsPanel->clear();
    m_completionModel->setItems({});
    m_breadcrumbs->clear();
    setWindowTitle("Zith Studio - Untitled");
}

void MainWindow::newProject()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "Choose project directory");
    if (dir.isEmpty()) return;

    bool ok;
    QString name = QInputDialog::getText(
        this, "Project Name", "Project name:",
        QLineEdit::Normal, QFileInfo(dir).fileName(), &ok);
    if (!ok || name.isEmpty()) return;

    QDir base(dir);
    base.mkpath("src");

    QFile toml(base.filePath("ZithProject.toml"));
    if (toml.open(QIODevice::WriteOnly | QIODevice::Text)) {
        toml.write(QString("[project]\nname = \"%1\"\nentry = \"src/main.zith\"\n").arg(name).toUtf8());
        toml.close();
    }

    QString mainPath = base.filePath("src/main.zith");
    QFile main(mainPath);
    if (main.open(QIODevice::WriteOnly | QIODevice::Text)) {
        main.write("// entry point\nfn main {\n    // ...\n}\n");
        main.close();
    }

    statusBar()->showMessage(QString("Created Zith project in %1").arg(dir), 5000);

    QDialog dlg(this);
    dlg.setWindowTitle("New Project");
    dlg.setMinimumWidth(350);
    auto *dlgLayout = new QVBoxLayout(&dlg);
    auto *dlgLabel = new QLabel(QString(
        "Project created at:<br><b>%1</b><br><br>What should happen to the current workspace?").arg(dir));
    dlgLabel->setWordWrap(true);
    dlgLayout->addWidget(dlgLabel);

    auto *btnBox = new QDialogButtonBox;
    auto *newWinBtn = btnBox->addButton("New Window", QDialogButtonBox::AcceptRole);
    auto *replaceBtn = btnBox->addButton("Replace", QDialogButtonBox::AcceptRole);
    auto *newCtxBtn = btnBox->addButton("New Context", QDialogButtonBox::AcceptRole);
    auto *cancelBtn = btnBox->addButton("Cancel", QDialogButtonBox::RejectRole);
    dlgLayout->addWidget(btnBox);

    int choice = -1;
    connect(newWinBtn, &QPushButton::clicked, &dlg, [&](){ choice = 0; dlg.accept(); });
    connect(replaceBtn, &QPushButton::clicked, &dlg, [&](){ choice = 1; dlg.accept(); });
    connect(newCtxBtn, &QPushButton::clicked, &dlg, [&](){ choice = 2; dlg.accept(); });
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    dlg.exec();

    if (choice == 0) {
        auto *win = new MainWindow;
        win->setAttribute(Qt::WA_DeleteOnClose);
        win->show();
    } else if (choice == 1) {
        saveContextState();
        m_contextManager->setCurrentRoot(dir);
        saveContextState();
        openFilePath(mainPath);
    } else if (choice == 2) {
        saveContextState();
        m_contextManager->appendNew(dir);
        openFilePath(mainPath);
    }
}

void MainWindow::openFile()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Open Zith File", {}, "Zith Files (*.zith);;All Files (*)");
    openFilePath(path);
}

void MainWindow::saveFile()
{
    auto *ed = currentEditor();
    if (!ed) return;

    if (ed->filePath().isEmpty()) {
        QString path = QFileDialog::getSaveFileName(
            this, "Save Zith File", {}, "Zith Files (*.zith);;All Files (*)");
        if (path.isEmpty()) return;
        ed->setFilePath(path);
    }

    QFile file(ed->filePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(ed->toPlainText().toUtf8());
        file.close();
        ed->document()->setModified(false);

        int idx = m_tabWidget->indexOf(ed);
        if (idx >= 0) {
            m_tabWidget->setTabText(idx, QFileInfo(ed->filePath()).fileName());
            m_tabWidget->setTabToolTip(idx, ed->filePath());
        }

        m_breadcrumbs->setPath(ed->filePath());

        if (m_lspClient && m_lspClient->isRunning())
            m_lspClient->saveDocument(ed->fileUri());
    }
}

void MainWindow::toggleFileTree(bool show)
{
    if (m_fileTreeAnimating) return;
    m_fileTreeAnimating = true;

    QList<int> sizes = m_splitter->sizes();
    int start = sizes[0];
    int end = show ? m_fileTreeWidth : 0;
    int total = sizes[0] + sizes[1];

    auto *timer = new QTimer(this);
    timer->setProperty("start", start);
    timer->setProperty("end", end);
    timer->setProperty("total", total);
    int steps = 10;
    timer->setProperty("steps", steps);
    timer->setProperty("step", 0);
    connect(timer, &QTimer::timeout, this, [this, timer, steps]() {
        int step = timer->property("step").toInt() + 1;
        timer->setProperty("step", step);
        int start = timer->property("start").toInt();
        int end = timer->property("end").toInt();
        int total = timer->property("total").toInt();

        qreal t = qreal(step) / steps;
        t = t * t * (3 - 2 * t);
        int w = int(start + (end - start) * t);
        if (step >= steps) {
            w = end;
            timer->stop();
            timer->deleteLater();
            m_fileTreeAnimating = false;
        }
        m_splitter->setSizes({w, total - w});
    });
    timer->start(16);
}

void MainWindow::saveContextState()
{
    int idx = m_contextManager->currentIndex();
    QStringList files;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *ed = qobject_cast<CodeEditor*>(m_tabWidget->widget(i));
        if (ed && !ed->filePath().isEmpty())
            files << ed->filePath();
    }
    m_contextManager->setContextState(idx, files, m_tabWidget->currentIndex());
}

void MainWindow::restoreContextState(const Context &ctx)
{
    while (m_tabWidget->count() > 0) {
        QWidget *w = m_tabWidget->widget(0);
        m_tabWidget->removeTab(0);
        w->deleteLater();
    }
    if (ctx.openFiles.isEmpty()) {
        createTab();
    } else {
        for (const QString &path : ctx.openFiles)
            openFilePath(path);
    }
    int idx = qMin(ctx.currentTab, m_tabWidget->count() - 1);
    if (idx >= 0)
        m_tabWidget->setCurrentIndex(idx);
}

CodeEditor *MainWindow::currentEditor() const
{
    return qobject_cast<CodeEditor*>(m_tabWidget->currentWidget());
}
