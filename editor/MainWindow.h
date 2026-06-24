#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QIcon>

class QTabWidget;
class QLabel;
class QSplitter;
class CodeEditor;
class SyntaxHighlighter;
class LspClient;
class LspCompletionModel;
class LspCompleter;
class DiagnosticsPanel;
class CompilerPanel;
class BreadcrumbsBar;
class FindReplaceBar;
class ActivityBar;
class FileTreePanel;
class ContextManager;
class SnippetManager;
struct Context;

QIcon createZithIcon();

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void openFilePath(const QString &path);

private slots:
    CodeEditor *createTab();
    void connectEditorSignals(CodeEditor *editor);
    void newFile();
    void newProject();
    void openFile();
    void saveFile();
    void toggleFileTree(bool show);
    void saveContextState();
    void restoreContextState(const Context &ctx);
    CodeEditor *currentEditor() const;

private:
    QTabWidget *m_tabWidget = nullptr;
    QMap<CodeEditor*, SyntaxHighlighter*> m_highlighters;
    LspClient *m_lspClient = nullptr;
    LspCompletionModel *m_completionModel = nullptr;
    LspCompleter *m_completer = nullptr;
    DiagnosticsPanel *m_diagnosticsPanel = nullptr;
    CompilerPanel *m_compilerPanel = nullptr;
    BreadcrumbsBar *m_breadcrumbs = nullptr;
    FindReplaceBar *m_findReplaceBar = nullptr;
    ActivityBar *m_activityBar = nullptr;
    FileTreePanel *m_fileTree = nullptr;
    ContextManager *m_contextManager = nullptr;
    SnippetManager *m_snippetManager = nullptr;
    bool m_initialContextSetup;
    QSplitter *m_splitter;
    int m_fileTreeWidth = 220;
    bool m_fileTreeAnimating = false;
    QLabel *m_lspLabel = nullptr;
    QLabel *m_contextLabel = nullptr;
    QLabel *m_errorLabel = nullptr;
    QLabel *m_posLabel = nullptr;
    QLabel *m_indentLabel = nullptr;
    QLabel *m_encodingLabel = nullptr;
    QLabel *m_langLabel = nullptr;
};

#endif
