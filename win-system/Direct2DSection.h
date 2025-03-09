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

#ifndef __DIRECT2DSECTION_H__
#define __DIRECT2DSECTION_H__

#include "util/CommonHeader.h"
// Direct2D 1.0 headers (Windows 7 SDK compatible)
#include <d2d1.h>
#include <dxgi.h>
#include <d2d1helper.h>
// No need for newer D2D headers that aren't in Windows 7 SDK
#include "win-system/Screen.h"

// Link with Windows 7 SDK compatible Direct2D libraries
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dxgi.lib")

// This class implements a Direct2D-based rendering system
// for improved performance over GDI's DIB section.
// Uses Direct2D 1.0 which is available in Windows 7 SDK.
class Direct2DSection
{
public:
  // Constructor creates a Direct2D render target.
  // compatibleWin is the window where rendering will be performed.
  Direct2DSection(const PixelFormat *pf, const Dimension *dim, HWND compatibleWin = 0);
  virtual ~Direct2DSection();

  // Get raw access to the bitmap buffer
  void *getBuffer();

  // Captures screen content to the Direct2D bitmap
  // This function copies a block of bits from the screen to the Direct2D bitmap
  void blitToDibSection(const Rect *rect);

  // Captures screen content including transparent windows
  void blitTransparentToDibSection(const Rect *rect);

  // Renders the captured content to the target window
  void blitFromDibSection(const Rect *rect);

  // Renders with stretching
  void stretchFromDibSection(const Rect *dstRect, const Rect *srcRect);

  // Debug method to make Direct2D rendering visible
  void drawTestPattern();

  // Draw a test pattern directly using Direct2D primitives
  void drawDirectTestPattern(const Rect *rect);

  // Draw a crosshair at a specific position for debugging mouse position
  void drawCrosshair(int x, int y, COLORREF color);

  void resize(const Rect* rect);

private:
  // Initialize Direct2D factory and resources
  void initDirect2D(const PixelFormat *pf, const Dimension *dim, HWND compatibleWin);
  
  // Release Direct2D resources
  void releaseDirect2D();

  // Creates a Direct2D bitmap compatible with the screen format
  void createBitmap(const PixelFormat *pf, const Dimension *dim);

  // Captures screen content to the Direct2D bitmap with specified flags
  void blitToDibSection(const Rect *rect, DWORD flags);

  // Renders to the window with specified flags
  void blitFromDibSection(const Rect *rect, DWORD flags);

  // Renders with stretching with specified flags
  void stretchFromDibSection(const Rect *srcRect, const Rect *dstRect, DWORD flags);

  // Helper function to update a Direct2D bitmap from DIB section memory
  bool UpdateBitmapFromDIB(ID2D1Bitmap* pBitmap, const Rect* rect, void* dibBits, UINT dibStride);

  // Direct2D resources
  ID2D1Factory* m_pD2DFactory;
  ID2D1RenderTarget* m_pRenderTarget;  // Use base interface to support different render target types
  ID2D1HwndRenderTarget* m_pHwndRenderTarget;
  ID2D1DCRenderTarget* m_pDCRenderTarget;
  ID2D1Bitmap* m_pBitmap;

  HWND m_hwnd;
  
  // Direct bitmap buffer (replaces GDI resources)
  void* m_bitmapBits;

  // render information
  int m_width;
  int m_height;
};

#endif // __DIRECT2DSECTION_H__ 