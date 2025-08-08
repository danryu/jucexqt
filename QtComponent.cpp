// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: BSD-3-Clause

#include "QtComponent.h"

#include <QtCore/qloggingcategory.h>
#include <QtGui/qwindow.h>

Q_LOGGING_CATEGORY(qtComponent, "juce.qt.component")

using namespace juce;

class QtComponent::Pimpl : public QObject, public ComponentMovementWatcher
{
public:
    Pimpl(QWindow *w, Component& c)
        : ComponentMovementWatcher(&c)
        , window(w)
    {
        qCDebug(qtComponent) << "Creating component" << this;

        window->installEventFilter(this);

        // Pick up initial state
        componentPeerChanged();
        componentVisibilityChanged();
    }

    ~Pimpl() override
    {
        qCDebug(qtComponent) << "Deleting component" << this;
    }

    void componentMovedOrResized(bool wasMoved, bool wasResized) override
    {
        Q_UNUSED(wasMoved);
        Q_UNUSED(wasResized);

        qCDebug(qtComponent) << "Component" << this << "moved or resized";

        auto *component = getComponent();

        if (!component->getWidth() || !component->getHeight()) {
            // Pick up size from window if not explicitly set on component
            auto size = window->size();
            component->setSize(size.width(), size.height());
        }

        // FIXME: The other native components use the top level component here,
        // which is weird, as the window is attached to our closest peer, so the
        // coordinates should presumably be relative to that.
        auto* peer = component->getPeer();
        if (!peer)
            return;

        auto area = peer->getAreaCoveredBy(*component);
        QRect geometry(area.getX(), area.getY(), area.getWidth(), area.getHeight());
        qCDebug(qtComponent) << "New geometry is" << geometry;
        window->setGeometry(geometry);
    }

    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object != window.get())
            return false;

        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Move: {
            auto g = window->geometry();
            Rectangle<int> bounds(g.x(), g.y(), g.width(), g.height());

            auto *component = getComponent();
            if (auto *peer = component->getPeer())
                bounds = component->getLocalArea(&peer->getComponent(), bounds);

            qCDebug(qtComponent) << "Component" << this << "observed"
                << event->type() << "with new geometry" << g;
            getComponent()->setBounds(bounds);
            break;
        }
        default:
            break;
        }

        return false;
    }

    void componentPeerChanged() override
    {
        std::cout << "=== QtComponent peer changed ===" << std::endl;

        auto *component = getComponent();
        auto *peer = component->getPeer();

        if (peer) {
            auto nativeHandle = peer->getNativeHandle();
            std::cout << "=== JUCE peer native handle: " << nativeHandle << " ===" << std::endl;
            
            // Re-parent before resetting foreign window, so that
            // the old foreign window doesn't destroy the window.
            auto parentWindow = QWindow::fromWinId(WId(nativeHandle));
            if (parentWindow) {
                std::cout << "=== Successfully created Qt parent window from JUCE handle ===" << std::endl;
                window->setParent(parentWindow);
                foreignWindow.reset(parentWindow);
            } else {
                std::cout << "=== Failed to create Qt parent window from JUCE handle ===" << std::endl;
                window->setParent(nullptr);
                foreignWindow.reset(nullptr);
            }
        } else {
            std::cout << "=== No JUCE peer found ===" << std::endl;
            window->setParent(nullptr);
            foreignWindow.reset(nullptr);
        }

        // ComponentMovementWatcher::componentParentHierarchyChanged() calls
        // componentMovedOrResized(true, true), but goes via the component overload,
        // which bails out of the component bounds have not changed, which they may
        // not have. The bounds of our wrapped component may still be dirty though,
        // now that we have a new peer, so force a move/resize.
        componentMovedOrResized(true, true);
    }

    void componentVisibilityChanged() override
    {
        std::cout << "=== QtComponent visibility changed ===" << std::endl;
        bool shouldShow = getComponent()->isShowing();
        std::cout << "=== Component showing: " << shouldShow << " ===" << std::endl;
        window->setVisible(shouldShow);
        if (shouldShow) {
            window->show();
            window->raise();
            window->requestUpdate(); // Force a paint event
            std::cout << "=== Qt window made visible, raised, and update requested ===" << std::endl;
        }
    }

    std::unique_ptr<QWindow> foreignWindow;
    std::unique_ptr<QWindow> window;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Pimpl)
};

QtComponent::QtComponent() = default;
QtComponent::~QtComponent() = default;

void QtComponent::setWindow(QWindow *window)
{
    if (window != getWindow())
        pimpl.reset(window ? new Pimpl(window, *this) : nullptr);
}

QWindow *QtComponent::getWindow() const
{
    return pimpl ? pimpl->window.get() : nullptr;
}
