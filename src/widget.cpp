#include "widget.h"

#include <qevent.h>

#include <QApplication>
#include <QDebug>
#include <QMainWindow>
#include <QScreen>
#include <QUrlQuery>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWindow>

#include "shell.h"

class NoContextMenuWebEngineView : public QWebEngineView {
    Q_OBJECT
   public:
    explicit NoContextMenuWebEngineView(QWidget* parent = nullptr) : QWebEngineView(parent) {}

   protected:
    void contextMenuEvent(QContextMenuEvent* event) override { event->ignore(); }
};

void WSS::Widget::Create(Shell& shell) {
    const size_t monitors = m_Info.Monitors.size();

    for (int i = 0; i < monitors; ++i) {
        auto monitorInfo = m_Info.Monitors[i];

        QString name = QString("wss.widget.%1.%2").arg(QString::fromStdString(m_Info.Name)).arg(monitorInfo.MonitorId);

        auto* window = new QWidget;
        window->setWindowTitle(name);
        window->setObjectName(name);
        window->setWindowFlag(Qt::FramelessWindowHint);
        window->setWindowFlag(Qt::WindowStaysOnTopHint);
        window->setAttribute(Qt::WA_TranslucentBackground);
        window->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        window->setScreen(QGuiApplication::screens().at(monitorInfo.MonitorId));
        window->show();
        window->hide();

        window->clearFocus();
        auto* webview = new NoContextMenuWebEngineView(window);

        // Load the URL
        QString uri = QString("http://localhost:%1").arg(shell.GetSettings().m_FrontendPort);
        if (!m_Info.Route.empty()) {
            uri += QString::fromStdString(m_Info.Route);
        }

        QUrl url(uri);
        QUrlQuery query;
        query.addQueryItem("widgetName", QString::fromStdString(m_Info.Name));
        query.addQueryItem("monitorId", QString::number(monitorInfo.MonitorId));
        url.setQuery(query);

        WSS_DEBUG("Loading URI: {}", url.toString().toStdString());
        if (m_Info.Route != "_DEBUG") {
            webview->load(url);
        } else {
            webview->load(QUrl("chrome://gpu"));
        }

        // Web engine settings
        QWebEngineSettings* settings = webview->settings();
        settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
        settings->setAttribute(QWebEngineSettings::WebGLEnabled, true);
        settings->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, true);

        // Transparent background support
        if (m_Info.Route != "_DEBUG") {
            webview->setAttribute(Qt::WA_TranslucentBackground);
            webview->setStyleSheet("background: transparent");
            webview->page()->setBackgroundColor(Qt::transparent);
        }

        // webview->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        // Move window to monitor
        const auto screens = QGuiApplication::screens();
        if (monitorInfo.MonitorId < screens.size()) {
            const QRect geometry = screens[monitorInfo.MonitorId]->geometry();

            int x = geometry.x() + monitorInfo.MarginLeft;
            int y = geometry.y() + monitorInfo.MarginTop;
            window->move(x, y);
        }

        window->setLayout(new QVBoxLayout);
        window->layout()->setContentsMargins(0, 0, 0, 0);
        window->layout()->addWidget(webview);
        window->resize(monitorInfo.Width, monitorInfo.Height);
        window->setContentsMargins(0, 0, 0, 0);

        QWindow* lwin = window->windowHandle();
        if (LayerShellQt::Window* lsh = LayerShellQt::Window::get(lwin)) {
            lsh->setScope("wss.shell");
            lsh->setLayer(static_cast<LayerShellQt::Window::Layer>(m_Info.Layer));
            lsh->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);

            lsh->setMargins(
                QMargins(monitorInfo.MarginLeft, monitorInfo.MarginTop, monitorInfo.MarginRight, monitorInfo.MarginBottom));
            LayerShellQt::Window::Anchors anchors;
            anchors.setFlag(LayerShellQt::Window::AnchorTop, IsAnchoredTo(WidgetAnchor::TOP));
            anchors.setFlag(LayerShellQt::Window::AnchorBottom, IsAnchoredTo(WidgetAnchor::BOTTOM));
            anchors.setFlag(LayerShellQt::Window::AnchorLeft, IsAnchoredTo(WidgetAnchor::LEFT));
            anchors.setFlag(LayerShellQt::Window::AnchorRight, IsAnchoredTo(WidgetAnchor::RIGHT));
            lsh->setAnchors(anchors);
            lsh->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);

            if (m_Info.DefaultHidden) {
                window->hide();
            } else {
                window->show();
                if (m_Info.Exclusivity) {
                    lsh->setExclusiveZone(m_Info.ExclusivityZone);
                }
            }
        }

        QRegion inputRegion;
        if (monitorInfo.ClickRegionMap.empty()) {
            // Since Qt mask doesn't really work with empty regions, we create a 1x1 region as a workaround.
            inputRegion = QRegion(0, 0, 1, 1);
        } else {
            for (const auto& [regionName, regionInfo] : monitorInfo.ClickRegionMap) {
                if (regionInfo.X == 0 && regionInfo.Y == 0 && regionInfo.Width == 0 && regionInfo.Height == 0) {
                    continue;
                }
                const QRect rect(regionInfo.X, regionInfo.Y, regionInfo.Width, regionInfo.Height);
                inputRegion += rect;
            }
        }
        window->setMask(inputRegion);
        window->update();

        m_Windows.emplace(monitorInfo.MonitorId, window);
        m_Views.emplace(monitorInfo.MonitorId, webview);

        WSS_DEBUG("Created widget '{}' on monitor ID: {}", m_Info.Name, monitorInfo.MonitorId);
    }
}
#include "widget.moc"