// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: BSD-3-Clause

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <QtCore/qloggingcategory.h>
#include <QtGui/qguiapplication.h>

Q_LOGGING_CATEGORY(qtEditor, "juce.qt.editor")

#if defined(QT_QUICK_LIB)

#include <QtQuick/qquickview.h>
#include <QtGui/qsurfaceformat.h>
#include <QtQml/qqmlengine.h>

class EditorWindow : public QQuickView
{
public:
    EditorWindow()
    {
        std::cout << "=== Creating QQuickView EditorWindow ===" << std::endl;
#ifdef __linux__
        #ifndef JUCE_STANDALONE_APPLICATION
        // Only set OpenGL surface format for plugins (VST3) - standalone uses software rendering
        QSurfaceFormat format;
        format.setDepthBufferSize(24);
        format.setStencilBufferSize(8);
        format.setVersion(3, 2);
        format.setProfile(QSurfaceFormat::CoreProfile);
        setFormat(format);
        std::cout << "=== Set OpenGL surface format for QQuickView (plugin mode) ===" << std::endl;
        #else
        std::cout << "=== Using software rendering, no OpenGL surface format needed ===" << std::endl;
        #endif
#endif
        setSource(QUrl("qrc:/main.qml"));
        std::cout << "=== QML status: " << status() << " (1=Ready, 2=Loading, 3=Error) ===" << std::endl;
        setResizeMode(QQuickView::SizeRootObjectToView);
        
        // Force continuous updates for standalone Linux
#ifdef __linux__
        connect(this, &QQuickView::afterRendering, [this]() {
            std::cout << "=== Qt Quick frame rendered ===" << std::endl;
        });
        
        // Force the render loop to start immediately
        update();
        requestUpdate();
        if (engine()) {
            engine()->clearComponentCache();
        }
        std::cout << "=== Forced initial render loop start ===" << std::endl;
#endif
        std::cout << "=== QQuickView created successfully ===" << std::endl;
    }
};

#else // Plain QRasterWindow

#include <QtGui/qrasterwindow.h>
#include <QtGui/qpainter.h>
#include <QtGui/qfont.h>

class EditorWindow : public QRasterWindow
{
public:
    EditorWindow()
    {
        std::cout << "=== Creating QRasterWindow EditorWindow ===" << std::endl;
        resize(400, 200);
        std::cout << "=== QRasterWindow created successfully ===" << std::endl;
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(QRect(0, 0, width(), height()), QGradient::DustyGrass);
        QFont font;
        font.setPointSize(36);
        p.setFont(font);
        p.drawText(QRectF(0, 0, width(), height()), Qt::AlignCenter, QStringLiteral("Hello Qt 🥳"));
    }
};

#endif

using namespace juce;

PluginEditor::PluginEditor(AudioProcessor& p)
    : AudioProcessorEditor(p)
{
    std::cout << "=== PluginEditor constructor ===" << std::endl;
#if defined(QT_QUICK_LIB)
    std::cout << "=== QT_QUICK_LIB is defined - using QQuickView ===" << std::endl;
#else
    std::cout << "=== QT_QUICK_LIB not defined - using QRasterWindow ===" << std::endl;
#endif

    setResizable(true, false);
    setWantsKeyboardFocus(true);

    if (!qGuiApp) {
#ifdef JUCE_STANDALONE_APPLICATION
        // For standalone, don't use AA_PluginApplication - be a real application
        std::cout << "Standalone mode: creating full QGuiApplication" << std::endl;
#else
        // For plugins, use plugin mode (this works in VST3)
        QCoreApplication::setAttribute(Qt::AA_PluginApplication);
#endif
        
#ifdef __linux__
        // For standalone Linux, force software rendering to avoid OpenGL context issues
        #ifdef JUCE_STANDALONE_APPLICATION
        std::cout << "=== Forcing software rendering for standalone Linux ===" << std::endl;
        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, true);
        QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL, false);
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES, false);
        qputenv("QT_QUICK_BACKEND", "software");
        qputenv("QSG_RHI_BACKEND", "software");
        #else
        // For plugins, use OpenGL as it works in VST3
        std::cout << "=== Using OpenGL rendering for plugin Linux ===" << std::endl;
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES, false);
        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, false);
        QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL, true);
        qputenv("QT_QUICK_BACKEND", "rhi");
        #endif
        
        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
        QCoreApplication::setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents, false);
        
        // Ensure proper platform plugin selection
        if (qgetenv("QT_QPA_PLATFORM").isEmpty()) {
            qputenv("QT_QPA_PLATFORM", "xcb"); // Force X11 backend
        }
        
        // Set additional environment variables for X11
        qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
#endif
        
        static int argc = 1; static char *argv[] = { const_cast<char*>("") };
        new QGuiApplication(argc, argv); // FIXME: Ref-count and dispose
    }

    m_qtComponent.setWindow(new EditorWindow);
    addAndMakeVisible(m_qtComponent);
    childBoundsChanged(&m_qtComponent);
}

PluginEditor::~PluginEditor()
{
    qCDebug(qtEditor) << "Deleting editor" << this;
}

void PluginEditor::resized()
{
    qCDebug(qtEditor) << "Editor" << this << "resized";
    m_qtComponent.setBounds(getLocalBounds());
}

void PluginEditor::childBoundsChanged(Component*)
{
    qCDebug(qtEditor) << "Editor" << this << "child bounds changed";
    setSize(m_qtComponent.getWidth(), m_qtComponent.getHeight());
}

void PluginEditor::paint(juce::Graphics&)
{
    // FIXME: We're still being asked to paint by JUCE. Ideally
    // there would be a way to tell JUCE to skip this component.
    qCDebug(qtEditor) << "Editor" << this << "asked to paint";
}
