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

#include "Direct2DSection.h"
#include "win-system/SystemException.h"
#include "rfb/PixelFormat.h"
#include "region/Rect.h"
#include "region/Dimension.h"
#include <algorithm>

/**
 * Helper function to create a Direct2D factory.
 * This is a standalone function to encapsulate the creation logic
 * and uses the Direct2D API from Windows 7 SDK.
 */
HRESULT CreateD2DFactory(ID2D1Factory** ppD2DFactory)
{
    // Create Direct2D factory with defaults for Windows 7 compatibility
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, ppD2DFactory);
    return hr;
}

Direct2DSection::Direct2DSection(const PixelFormat *pf, const Dimension *dim, HWND compatibleWin)
: m_pD2DFactory(NULL),
  m_pRenderTarget(NULL),
  m_pBitmap(NULL),
  m_screenDC(NULL),
  m_memDC(NULL),
  m_hBitmap(NULL),
  m_hOldBitmap(NULL),
  m_pBits(NULL),
  m_width(dim->width),
  m_height(dim->height),
  m_srcOffsetX(0),
  m_srcOffsetY(0)
{
  try {
    initDirect2D(pf, dim, compatibleWin);
  } catch (...) {
    releaseDirect2D();
    throw;
  }
}

Direct2DSection::~Direct2DSection()
{
  try {
    releaseDirect2D();
  } catch (...) {
    // Ignore exceptions in destructor
  }
}

void *Direct2DSection::getBuffer()
{
  return m_pBits;
}

void Direct2DSection::blitToDibSection(const Rect *rect)
{
  blitToDibSection(rect, SRCCOPY);
}

void Direct2DSection::blitTransparentToDibSection(const Rect *rect)
{
  blitToDibSection(rect, SRCCOPY | CAPTUREBLT);
}

void Direct2DSection::blitFromDibSection(const Rect *rect)
{
  blitFromDibSection(rect, SRCCOPY);
}

void Direct2DSection::stretchFromDibSection(const Rect *srcRect, const Rect *dstRect)
{
  stretchFromDibSection(srcRect, dstRect, SRCCOPY);
}

/**
 * Captures screen content to the Direct2D bitmap
 * This is the core function for screen capture using Direct2D
 * 
 * Important: Currently the capture process still uses GDI's BitBlt
 * because Direct2D doesn't provide direct screen capture functionality.
 * The GDI bitmap data is then transferred to the Direct2D bitmap.
 * 
 * For total GDI elimination, a future implementation could use 
 * Desktop Duplication API (Windows 8+) or Windows Graphics Capture (Windows 10+)
 */
void Direct2DSection::blitToDibSection(const Rect *rect, DWORD flags)
{
  // First capture using GDI (temporary until we implement Direct2D capture)
  if (BitBlt(m_memDC, rect->left, rect->top, rect->getWidth(), rect->getHeight(),
           m_screenDC, rect->left + m_srcOffsetX, rect->top + m_srcOffsetY, flags) == 0) {
    throw SystemException(_T("Failed to capture screen to GDI bitmap"));
  }

  // Begin drawing
  m_pRenderTarget->BeginDraw();

  // Update Direct2D bitmap with captured data
  D2D1_RECT_U d2dRect = D2D1::RectU(rect->left, rect->top,
                                    rect->left + rect->getWidth(),
                                    rect->top + rect->getHeight());

  HRESULT hr = m_pBitmap->CopyFromMemory(&d2dRect, m_pBits,
                                         m_width * 4); // Assuming 32bpp for now
  
  if (FAILED(hr)) {
    m_pRenderTarget->EndDraw();
    throw SystemException(_T("Failed to copy GDI bitmap to Direct2D bitmap"));
  }

  m_pRenderTarget->EndDraw();
}

/**
 * Renders the captured content to the target window using Direct2D
 * This provides hardware-accelerated rendering which is faster than GDI
 */
void Direct2DSection::blitFromDibSection(const Rect *rect, DWORD flags)
{
  // Begin drawing
  m_pRenderTarget->BeginDraw();

  // Clear the background (optional)
  m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

  // Draw the bitmap to the render target
  // Use Windows 7 compatible parameters
  D2D1_RECT_F destRect = D2D1::RectF(
    static_cast<FLOAT>(rect->left), 
    static_cast<FLOAT>(rect->top),
    static_cast<FLOAT>(rect->left + rect->getWidth()),
    static_cast<FLOAT>(rect->top + rect->getHeight())
  );

  // Windows 7 compatible bitmap drawing
  m_pRenderTarget->DrawBitmap(
    m_pBitmap,                               // Bitmap to draw
    destRect,                                // Destination rectangle
    1.0f,                                    // Opacity
    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,  // Windows 7 compatible mode
    NULL                                     // No source rectangle, use the entire bitmap
  );

  // End drawing
  HRESULT hr = m_pRenderTarget->EndDraw();
  if (FAILED(hr)) {
    throw SystemException(_T("Failed to render Direct2D bitmap to window"));
  }
}

/**
 * Renders with stretching using Direct2D's high-quality scaling
 * This is much higher quality than GDI's StretchBlt and is accelerated
 */
void Direct2DSection::stretchFromDibSection(const Rect *srcRect, const Rect *dstRect, DWORD flags)
{
  // Begin drawing
  m_pRenderTarget->BeginDraw();

  // Clear the background (optional)
  m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

  // Draw the bitmap to the render target with stretching
  D2D1_RECT_F destRect = D2D1::RectF(
    static_cast<FLOAT>(dstRect->left),
    static_cast<FLOAT>(dstRect->top),
    static_cast<FLOAT>(dstRect->left + dstRect->getWidth()),
    static_cast<FLOAT>(dstRect->top + dstRect->getHeight())
  );

  // Windows 7 compatible stretched bitmap drawing
  m_pRenderTarget->DrawBitmap(
    m_pBitmap,                             // Bitmap to draw
    destRect,                              // Destination rectangle
    1.0f,                                  // Opacity
    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, // Use linear interpolation for scaling
    NULL                                   // No source rectangle, use the entire bitmap
  );

  // End drawing
  HRESULT hr = m_pRenderTarget->EndDraw();
  if (FAILED(hr)) {
    throw SystemException(_T("Failed to render stretched Direct2D bitmap to window"));
  }
}

/**
 * Initialize Direct2D subsystem with Windows 7 compatible settings
 * This creates the factory, render target, and bitmap needed for Direct2D rendering
 */
void Direct2DSection::initDirect2D(const PixelFormat *pf, const Dimension *dim, HWND compatibleWin)
{
  // For prototype: If compatibleWin is NULL, fallback to using the entire desktop instead of throwing
  if (compatibleWin == 0) {
    compatibleWin = GetDesktopWindow();
    if (compatibleWin == 0) {
      throw SystemException(_T("Failed to get desktop window for Direct2D initialization"));
    }
  }

  // Verify the window is valid
  if (!IsWindow(compatibleWin)) {
    throw SystemException(_T("Invalid window handle for Direct2D initialization"));
  }

  // Get screen info if needed
  m_screen.update();
  Rect deskRect = m_screen.getDesktopRect();
  m_srcOffsetX = deskRect.left;
  m_srcOffsetY = deskRect.top;

  // Create Direct2D factory
  HRESULT hr = CreateD2DFactory(&m_pD2DFactory);
  if (FAILED(hr)) {
    throw SystemException(_T("Failed to create Direct2D factory"));
  }

  // Create render target with Windows 7 compatible settings
  // Make sure dimensions are at least 1x1 to avoid D2D errors
  UINT width = std::max(1, dim->width);
  UINT height = std::max(1, dim->height);
  D2D1_SIZE_U size = D2D1::SizeU(width, height);
  
  // First try creating a HWND render target (hardware accelerated)
  // Use specific settings that work well with Windows 7
  D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
    D2D1_RENDER_TARGET_TYPE_DEFAULT,
    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
    0.0f, // Default DPI
    0.0f, // Default DPI
    D2D1_RENDER_TARGET_USAGE_NONE, // No specific usage flags
    D2D1_FEATURE_LEVEL_DEFAULT     // Use default feature level (Direct2D 1.0 in Windows 7)
  );
  
  D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
    compatibleWin,
    size,
    D2D1_PRESENT_OPTIONS_IMMEDIATELY  // Windows 7 SDK compatible option
  );

  // Try to create the render target (as ID2D1RenderTarget interface)
  hr = m_pD2DFactory->CreateHwndRenderTarget(
    rtProps,
    hwndProps,
    reinterpret_cast<ID2D1HwndRenderTarget**>(&m_pRenderTarget)
  );

  if (FAILED(hr)) {
    // If HWND render target fails, try a compatible fallback approach
    D2D1_SIZE_F sizeF = D2D1::SizeF(static_cast<float>(width), static_cast<float>(height));
    D2D1_PIXEL_FORMAT pixFormat = D2D1::PixelFormat(
      DXGI_FORMAT_B8G8R8A8_UNORM,
      D2D1_ALPHA_MODE_IGNORE
    );
    
    // Try to create a DCRenderTarget - this is often more compatible
    D2D1_RENDER_TARGET_PROPERTIES dcProps = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      pixFormat,
      96.0f, 96.0f, // Standard DPI
      D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE, // GDI compatible for better integration
      D2D1_FEATURE_LEVEL_DEFAULT
    );
    
    ID2D1DCRenderTarget* pDCRT = NULL;
    hr = m_pD2DFactory->CreateDCRenderTarget(&dcProps, &pDCRT);
    
    if (SUCCEEDED(hr) && pDCRT) {
      // If we created a DC render target successfully, bind it to the window DC temporarily
      HDC hdc = GetDC(compatibleWin);
      if (hdc) {
        RECT rc = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
        pDCRT->BindDC(hdc, &rc);
        ReleaseDC(compatibleWin, hdc);
      }
      
      // No need for a cast here since m_pRenderTarget is now ID2D1RenderTarget*
      m_pRenderTarget = pDCRT;
    } else {
      throw SystemException(_T("Failed to create any compatible Direct2D render target"));
    }
  }

  // Create bitmap format - Use DXGI_FORMAT_B8G8R8A8_UNORM which is supported in Windows 7
  D2D1_PIXEL_FORMAT pixelFormat = D2D1::PixelFormat(
    DXGI_FORMAT_B8G8R8A8_UNORM,  // Windows 7 compatible format
    D2D1_ALPHA_MODE_IGNORE       // Ignore alpha
  );

  // Create bitmap
  D2D1_BITMAP_PROPERTIES bitmapProps = D2D1::BitmapProperties(pixelFormat);
  hr = m_pRenderTarget->CreateBitmap(
    size,
    NULL,
    0,
    bitmapProps,
    &m_pBitmap
  );

  if (FAILED(hr)) {
    throw SystemException(_T("Failed to create Direct2D bitmap"));
  }

  // Create GDI resources for screen capture (temporary until we implement Direct2D capture)
  m_screenDC = GetDC(NULL);  // Get screen DC
  if (!m_screenDC) {
    throw SystemException(_T("Failed to get screen DC"));
  }
  
  m_memDC = CreateCompatibleDC(m_screenDC);
  if (!m_memDC) {
    throw SystemException(_T("Failed to create compatible DC"));
  }

  // Create DIB for screen capture
  BITMAPINFO bmi = {0};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;  // Top-down DIB
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;          // 32bpp
  bmi.bmiHeader.biCompression = BI_RGB;
  
  m_hBitmap = CreateDIBSection(m_memDC, &bmi, DIB_RGB_COLORS, &m_pBits, NULL, 0);
  if (!m_hBitmap) {
    throw SystemException(_T("Failed to create DIB section"));
  }
  
  m_hOldBitmap = (HBITMAP)SelectObject(m_memDC, m_hBitmap);
}

/**
 * Clean up all Direct2D and GDI resources
 */
void Direct2DSection::releaseDirect2D()
{
  // Release GDI resources
  if (m_hOldBitmap) {
    SelectObject(m_memDC, m_hOldBitmap);
    m_hOldBitmap = NULL;
  }
  
  if (m_hBitmap) {
    DeleteObject(m_hBitmap);
    m_hBitmap = NULL;
  }
  
  if (m_memDC) {
    DeleteDC(m_memDC);
    m_memDC = NULL;
  }
  
  if (m_screenDC) {
    ReleaseDC(NULL, m_screenDC);
    m_screenDC = NULL;
  }

  // Release Direct2D resources
  if (m_pBitmap) {
    m_pBitmap->Release();
    m_pBitmap = NULL;
  }
  
  // Release the render target (works for both HwndRenderTarget and DCRenderTarget)
  if (m_pRenderTarget) {
    m_pRenderTarget->Release();
    m_pRenderTarget = NULL;
  }
  
  if (m_pD2DFactory) {
    m_pD2DFactory->Release();
    m_pD2DFactory = NULL;
  }
} 