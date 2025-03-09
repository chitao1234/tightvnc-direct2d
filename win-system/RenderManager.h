// Copyright (C) 2023
// All rights reserved.
//
//-------------------------------------------------------------------------
// This file is part of the TightVNC software.  Please visit our Web site:
//
//                       http://www.tightvnc.com/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//-------------------------------------------------------------------------
//

#ifndef __RENDERMANAGER_H__
#define __RENDERMANAGER_H__

#include "util/CommonHeader.h"
#include "rfb/PixelFormat.h"
#include "region/Rect.h"
#include "region/Dimension.h"
#include "win-system/DibSection.h"
#include "win-system/Direct2DSection.h"

// Render mode enumeration
enum RenderMode {
  RENDER_MODE_GDI,     // Legacy GDI rendering
  RENDER_MODE_DIRECT2D // Hardware-accelerated Direct2D rendering
};

// This class manages rendering for TightVNC viewer
// providing an option to switch between GDI and Direct2D
class RenderManager
{
public:
  // Create a render manager with specified mode
  RenderManager(const PixelFormat *pf, const Dimension *dim, 
                HWND compatibleWin, 
                RenderMode mode = RENDER_MODE_GDI);
  virtual ~RenderManager();

  // Get raw access to the bitmap buffer
  void *getBuffer();

  // Captures screen content
  void blitToDibSection(const Rect *rect);

  // Captures screen content including transparent windows
  void blitTransparentToDibSection(const Rect *rect);

  // Renders the captured content to the target window
  void blitFromDibSection(const Rect *rect);

  // Renders with stretching
  void stretchFromDibSection(const Rect *srcRect, const Rect *dstRect);

  // Get current render mode
  RenderMode getRenderMode() const { return m_mode; }

  // Set render mode (GDI or Direct2D)
  // Returns false if the requested mode is not available
  bool setRenderMode(RenderMode mode);

  /**
   * Checks if Direct2D is available on this system
   * @return true if Direct2D is available, false otherwise
   */
  static bool isD2DAvailable();

  // NEW FUNCTION: handle resize
  void resize(const Rect* rect);

private:
  // Create/destroy render implementations
  void createRenderer();
  void destroyRenderer();

  // Current rendering mode
  RenderMode m_mode;

  // Rendering implementation objects
  DibSection* m_dibSection;
  Direct2DSection* m_direct2DSection;

  // Configuration data
  PixelFormat m_pixelFormat;
  Dimension m_dimension;
  HWND m_window;
};

#endif // __RENDERMANAGER_H__ 