// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_H_
#define UI_AURA_DESKTOP_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/cursor.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/window.h"
#include "ui/base/events.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

namespace gfx {
class Size;
}

namespace aura {

class DesktopDelegate;
class DesktopHost;
class DesktopObserver;
class KeyEvent;
class MouseEvent;
class TouchEvent;

// Desktop is responsible for hosting a set of windows.
class AURA_EXPORT Desktop : public ui::CompositorDelegate,
                            public Window,
                            public internal::FocusManager {
 public:
  Desktop();
  virtual ~Desktop();

  static Desktop* GetInstance();
  static void DeleteInstanceForTesting();

  static void set_compositor_factory_for_testing(ui::Compositor*(*factory)()) {
    compositor_factory_ = factory;
  }
  static ui::Compositor* (*compositor_factory())() {
    return compositor_factory_;
  }

  ui::Compositor* compositor() { return compositor_.get(); }
  gfx::Point last_mouse_location() const { return last_mouse_location_; }
  DesktopDelegate* delegate() { return delegate_.get(); }
  Window* active_window() { return active_window_; }
  Window* mouse_pressed_handler() { return mouse_pressed_handler_; }
  Window* capture_window() { return capture_window_; }

  void SetDelegate(DesktopDelegate* delegate);

  // Shows the desktop host.
  void ShowDesktop();

  // Sets the size of the desktop.
  void SetHostSize(const gfx::Size& size);
  gfx::Size GetHostSize() const;

  // Shows the specified cursor.
  void SetCursor(gfx::NativeCursor cursor);

  // Shows the desktop host and runs an event loop for it.
  void Run();

  // Draws the necessary set of windows.
  void Draw();

  // Handles a mouse event. Returns true if handled.
  bool OnMouseEvent(const MouseEvent& event);

  // Handles a key event. Returns true if handled.
  bool OnKeyEvent(const KeyEvent& event);

  // Handles a touch event. Returns true if handled.
  bool OnTouchEvent(const TouchEvent& event);

  // Called when the host changes size.
  void OnHostResized(const gfx::Size& size);

  // Sets the active window to |window| and the focused window to |to_focus|.
  // If |to_focus| is NULL, |window| is focused.
  void SetActiveWindow(Window* window, Window* to_focus);

  // Activates the topmost window. Does nothing if the topmost window is already
  // active.
  void ActivateTopmostWindow();

  // Deactivates |window| and activates the topmost window. Does nothing if
  // |window| is not a topmost window, or there are no other suitable windows to
  // activate.
  void Deactivate(Window* window);

  // Invoked when |window| is being destroyed.
  void WindowDestroying(Window* window);

  // Returns the desktop's dispatcher. The result should only be passed to
  // MessageLoopForUI::Run() or MessageLoopForUI::RunAllPendingWithDispatcher(),
  // or used to dispatch an event by |Dispatch(const NativeEvent&)| on it.
  // It must never be stored.
  MessageLoop::Dispatcher* GetDispatcher();

  // Add/remove observer.
  void AddObserver(DesktopObserver* observer);
  void RemoveObserver(DesktopObserver* observer);

  // Sets capture to the specified window.
  void SetCapture(Window* window);

  // If |window| has mouse capture, the current capture window is set to NULL.
  void ReleaseCapture(Window* window);

 private:
  // Called whenever the mouse moves, tracks the current |mouse_moved_handler_|,
  // sending exited and entered events as its value changes.
  void HandleMouseMoved(const MouseEvent& event, Window* target);

  // Overridden from ui::CompositorDelegate
  virtual void ScheduleDraw();

  // Overridden from Window:
  virtual bool CanFocus() const OVERRIDE;
  virtual internal::FocusManager* GetFocusManager() OVERRIDE;
  virtual Desktop* GetDesktop() OVERRIDE;

  // Overridden from FocusManager:
  virtual void SetFocusedWindow(Window* window) OVERRIDE;
  virtual Window* GetFocusedWindow() OVERRIDE;
  virtual bool IsFocusedWindow(const Window* window) const OVERRIDE;

  // Initializes the desktop.
  void Init();

  scoped_refptr<ui::Compositor> compositor_;

  scoped_ptr<DesktopHost> host_;

  scoped_ptr<DesktopDelegate> delegate_;

  static Desktop* instance_;

  // Used to schedule painting.
  base::WeakPtrFactory<Desktop> schedule_paint_factory_;

  // Factory used to create Compositors. Settable by tests.
  static ui::Compositor*(*compositor_factory_)();

  Window* active_window_;

  // Last location seen in a mouse event.
  gfx::Point last_mouse_location_;

  // Are we in the process of being destroyed? Used to avoid processing during
  // destruction.
  bool in_destructor_;

  ObserverList<DesktopObserver> observers_;

  // The capture window. When not-null, this window receives all the mouse and
  // touch events.
  Window* capture_window_;

  Window* mouse_pressed_handler_;
  Window* mouse_moved_handler_;
  Window* focused_window_;
  Window* touch_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(Desktop);
};

}  // namespace aura

#endif  // UI_AURA_DESKTOP_H_
