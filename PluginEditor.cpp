// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: BSD-3-Clause

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <QtCore/qloggingcategory.h>
#include <QtGui/qguiapplication.h>
#include <iostream>

Q_LOGGING_CATEGORY(qtEditor, "juce.qt.editor")

#if defined(QT_QUICK_LIB)

#include <QtQuick/qquickview.h>
#include <QtGui/qsurfaceformat.h>

class EditorWindow : public QQuickView
{
public:
    EditorWindow()
    {
        std::cout << "=== Creating QQuickView EditorWindow ===" << std::endl;
#ifdef __linux__
        // Ensure proper OpenGL context format on Linux
        QSurfaceFormat format;
        format.setDepthBufferSize(24);
        format.setStencilBufferSize(8);
        format.setVersion(3, 2);
        format.setProfile(QSurfaceFormat::CoreProfile);
        setFormat(format);
        std::cout << "=== Set OpenGL surface format for Linux ===" << std::endl;
#endif
        setSource(QUrl("qrc:/main.qml"));
        std::cout << "=== QML source set to qrc:/main.qml, status: " << status() << " ===" << std::endl;
        setResizeMode(QQuickView::SizeRootObjectToView);
        std::cout << "=== QQuickView resize mode set ===" << std::endl;
    }
};

#else // Plain QRasterWindow

#include <QtGui/qrasterwindow.h>
#include <QtGui/qpainter.h>
#include <QtGui/qfont.h>
#include <QtGui/qevent.h>

class EditorWindow : public QRasterWindow
{
public:
    EditorWindow()
    {
        std::cout << "=== Creating QRasterWindow EditorWindow ===" << std::endl;
        resize(400, 200);
        std::cout << "=== QRasterWindow resized to 400x200 ===" << std::endl;
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        std::cout << "=== QRasterWindow paintEvent called ===" << std::endl;
        QPainter p(this);
        p.fillRect(QRect(0, 0, width(), height()), QGradient::DustyGrass);
        QFont font;
        font.setPointSize(36);
        p.setFont(font);
        p.drawText(QRectF(0, 0, width(), height()), Qt::AlignCenter, QStringLiteral("Hello Qt 🥳"));
        std::cout << "=== QRasterWindow paint completed ===" << std::endl;
    }
};

#endif

using namespace juce;

PluginEditor::PluginEditor(AudioProcessor& p)
    : AudioProcessorEditor(p)
{
    std::cout << "=== PluginEditor::PluginEditor() called ===" << std::endl;

    setResizable(true, false);
    setWantsKeyboardFocus(true);

    if (!qGuiApp) {
        std::cout << "=== No existing QGuiApplication found ===" << std::endl;
#ifdef JUCE_STANDALONE_APPLICATION
        // For standalone applications, don't use AA_PluginApplication
        // This is the key difference from VST3 plugin behavior
        std::cout << "=== Initializing Qt for standalone application ===" << std::endl;
#ifdef __linux__
        // Linux-specific standalone application setup
        QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL, true);
        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
        std::cout << "=== Set Linux standalone Qt attributes ===" << std::endl;
#endif
#else
        // For plugins (VST3, AU, etc.), use plugin mode
        QCoreApplication::setAttribute(Qt::AA_PluginApplication);
        std::cout << "=== Initializing Qt for plugin application ===" << std::endl;
#endif
        
#ifdef __linux__
        // Linux-specific fixes for OpenGL context and window management
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES, false);
        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, false);
        QCoreApplication::setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents, false);
        
        // Ensure proper platform plugin selection
        if (qgetenv("QT_QPA_PLATFORM").isEmpty()) {
            qputenv("QT_QPA_PLATFORM", "xcb"); // Force X11 backend
            std::cout << "=== Set QT_QPA_PLATFORM to xcb ===" << std::endl;
        }
        
        // Set additional environment variables for X11
        qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
        qputenv("QT_QUICK_BACKEND", "rhi");
        std::cout << "=== Set Linux-specific Qt environment variables ===" << std::endl;
#endif
        
        static int argc = 1; static char *argv[] = { const_cast<char*>("") };
        auto* app = new QGuiApplication(argc, argv); // FIXME: Ref-count and dispose
        std::cout << "=== Created QGuiApplication: " << app << " ===" << std::endl;
        std::cout << "=== Qt platform plugin: " << app->platformName().toStdString() << " ===" << std::endl;
    } else {
        std::cout << "=== Using existing QGuiApplication: " << qGuiApp << " ===" << std::endl;
        std::cout << "=== Qt platform plugin: " << qGuiApp->platformName().toStdString() << " ===" << std::endl;
    }

#if defined(QT_QUICK_LIB)
    std::cout << "=== Using Qt Quick (QQuickView) for rendering ===" << std::endl;
#else
    std::cout << "=== Using Qt Raster (QRasterWindow) for rendering ===" << std::endl;
#endif

    auto* editorWindow = new EditorWindow;
    std::cout << "=== Created EditorWindow: " << editorWindow << " ===" << std::endl;
    
#ifdef __linux__
    // Linux standalone: Force window to show as top-level first to test painting
    editorWindow->show();
    editorWindow->raise();
    editorWindow->requestUpdate();
    std::cout << "=== Forced Qt window to show as top-level ===" << std::endl;
#endif
    
    m_qtComponent.setWindow(editorWindow);
    addAndMakeVisible(m_qtComponent);
    childBoundsChanged(&m_qtComponent);
    
    std::cout << "=== Editor setup complete, window size: " << editorWindow->size().width() 
              << "x" << editorWindow->size().height() << " ===" << std::endl;
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
