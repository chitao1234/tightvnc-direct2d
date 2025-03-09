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

#include "win-system/Direct2DSection.h"
#include "win-system/SystemException.h"
#include "rfb/PixelFormat.h"
#include "region/Rect.h"
#include "region/Dimension.h"
#include <algorithm>
#include <stdio.h>
// Add DWrite header for text rendering
#include <dwrite.h>
// Link with the DWrite library
#pragma comment(lib, "dwrite.lib")

// Debug logging
#define DEBUG_LOG(msg, ...) { FILE* f = fopen("d2d_debug.log", "a"); if(f) { fprintf(f, "[D2D] " msg "\n", ##__VA_ARGS__); fclose(f); } }

#define LOG_D2D(fmt, ...) {\
  FILE *f = fopen("d2d_debug.log", "a");\
  if (f) {\
    fprintf(f, "[%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__);\
    fclose(f);\
  }\
}

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
  m_width(dim->width),
  m_height(dim->height),
  m_bitmapBits(NULL),
  m_pDCRenderTarget(NULL),
  m_pHwndRenderTarget(NULL)
{
  DEBUG_LOG("Creating Direct2DSection, dimensions: %dx%d", dim->width, dim->height);
  try {
    initDirect2D(pf, dim, compatibleWin);
    DEBUG_LOG("Direct2DSection initialized successfully");
  } catch (SystemException& e) {
    DEBUG_LOG("Exception in Direct2DSection constructor: %s", e.getMessage());
    releaseDirect2D();
    throw;
  } catch (...) {
    DEBUG_LOG("Unknown exception in Direct2DSection constructor");
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
  return m_bitmapBits;
}

void Direct2DSection::blitToDibSection(const Rect *rect)
{
  throw SystemException(_T("blitToDibSection is not supported in the Direct2D implementation"));
}

void Direct2DSection::blitTransparentToDibSection(const Rect *rect)
{
  throw SystemException(_T("blitTransparentToDibSection is not supported in the Direct2D implementation"));
}

void Direct2DSection::blitFromDibSection(const Rect *rect)
{
  blitFromDibSection(rect, 0);
}

void Direct2DSection::blitFromDibSection(const Rect *rect, DWORD flags)
{
  LOG_D2D("blitFromDibSection called with rect=(%d,%d,%d,%d), flags=%lu",
          rect->left, rect->top, rect->right, rect->bottom, flags);

  if (m_pRenderTarget == nullptr || m_pBitmap == nullptr) {
    LOG_D2D("Error: Render target or bitmap is null");
    return;
  }

  // Get render target size to ensure we're drawing within bounds
  D2D1_SIZE_F rtSize = m_pRenderTarget->GetSize();
  LOG_D2D("Render target size: %.2f x %.2f", rtSize.width, rtSize.height);
  
  // Get bitmap size
  D2D1_SIZE_U bitmapSize = m_pBitmap->GetPixelSize();
  LOG_D2D("Bitmap size: %u x %u", bitmapSize.width, bitmapSize.height);

  // BeginDraw returns void
  m_pRenderTarget->BeginDraw();

  // Calculate source area within the buffer
  Rect sourceArea;
  sourceArea.left = rect->left;
  sourceArea.top = rect->top;
  sourceArea.right = rect->right;
  sourceArea.bottom = rect->bottom;
  
  // Adjust source area to fit within bitmap bounds
  if (sourceArea.right > (int)bitmapSize.width) sourceArea.right = bitmapSize.width;
  if (sourceArea.bottom > (int)bitmapSize.height) sourceArea.bottom = bitmapSize.height;
  
  LOG_D2D("Adjusted source area to bitmap bounds: (%d,%d,%d,%d)",
          sourceArea.left, sourceArea.top, sourceArea.right, sourceArea.bottom);

  // Update the bitmap from the buffer
  HRESULT hr = m_pBitmap->CopyFromMemory(
      nullptr,  // Copy the entire bitmap
      m_bitmapBits,
      m_width * 4
  );
  
  if (FAILED(hr)) {
    LOG_D2D("Failed to update bitmap data: 0x%08x", hr);
    m_pRenderTarget->EndDraw();
    return;
  }

  // Clear the render target with a black background
  m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

  // Calculate destination rectangle - where to draw in the window
  D2D1_RECT_F d2dDstRect = D2D1::RectF(
    static_cast<FLOAT>(rect->left),
    static_cast<FLOAT>(rect->top),
    static_cast<FLOAT>(rect->right),
    static_cast<FLOAT>(rect->bottom)
  );

  // Ensure destination rectangle is within render target bounds
  if (d2dDstRect.right > rtSize.width) {
    LOG_D2D("Destination right edge exceeds render target width, adjusting from %.2f to %.2f", 
            d2dDstRect.right, rtSize.width);
    d2dDstRect.right = rtSize.width;
  }
  
  if (d2dDstRect.bottom > rtSize.height) {
    LOG_D2D("Destination bottom edge exceeds render target height, adjusting from %.2f to %.2f", 
            d2dDstRect.bottom, rtSize.height);
    d2dDstRect.bottom = rtSize.height;
  }

  // Create source rectangle from the bitmap
  D2D1_RECT_F d2dSrcRect = D2D1::RectF(
    static_cast<FLOAT>(sourceArea.left),
    static_cast<FLOAT>(sourceArea.top),
    static_cast<FLOAT>(sourceArea.right),
    static_cast<FLOAT>(sourceArea.bottom)
  );

  LOG_D2D("Drawing bitmap - source: (%.2f,%.2f,%.2f,%.2f), dest: (%.2f,%.2f,%.2f,%.2f)",
          d2dSrcRect.left, d2dSrcRect.top, d2dSrcRect.right, d2dSrcRect.bottom,
          d2dDstRect.left, d2dDstRect.top, d2dDstRect.right, d2dDstRect.bottom);

  // Draw the bitmap to the render target without stretching
  m_pRenderTarget->DrawBitmap(
    m_pBitmap,
    d2dDstRect,
    1.0f,           // Opacity
    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, // No interpolation for 1:1 blitting
    d2dSrcRect
  );

  hr = m_pRenderTarget->EndDraw();
  if (FAILED(hr)) {
    LOG_D2D("EndDraw failed with error: 0x%08x", hr);
    return;
  }

  LOG_D2D("blitFromDibSection completed successfully");
}

void Direct2DSection::stretchFromDibSection(const Rect *dstRect, const Rect *srcRect)
{
  // tightvnc made me do this...
  stretchFromDibSection(srcRect, dstRect, 0);
}

void Direct2DSection::stretchFromDibSection(const Rect *srcRect, const Rect *dstRect, DWORD flags)
{
  LOG_D2D("stretchFromDibSection with flags called: src=(%d,%d,%d,%d), dst=(%d,%d,%d,%d), flags=%lu", 
         srcRect->left, srcRect->top, srcRect->right, srcRect->bottom,
         dstRect->left, dstRect->top, dstRect->right, dstRect->bottom, flags);

  if (m_pRenderTarget == nullptr || m_pBitmap == nullptr) {
    LOG_D2D("Error: Render target or bitmap is null");
    return;
  }

  // Get render target size to ensure we're drawing within bounds
  D2D1_SIZE_F rtSize = m_pRenderTarget->GetSize();
  LOG_D2D("Render target size: %.2f x %.2f", rtSize.width, rtSize.height);
  
  // Get bitmap size
  D2D1_SIZE_U bitmapSize = m_pBitmap->GetPixelSize();
  LOG_D2D("Bitmap size: %u x %u", bitmapSize.width, bitmapSize.height);

  // BeginDraw returns void
  m_pRenderTarget->BeginDraw();
  
  // Update the bitmap from the buffer (always use the full bitmap)
  HRESULT hr = m_pBitmap->CopyFromMemory(
      nullptr,  // Copy the entire bitmap
      m_bitmapBits,
      m_width * 4
  );
  
  if (FAILED(hr)) {
    LOG_D2D("Failed to update bitmap data: 0x%08x", hr);
    m_pRenderTarget->EndDraw();
    return;
  }

  // Clear the render target
  m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

  // Calculate destination rectangle in the window (where to draw)
  D2D1_RECT_F d2dDstRect = D2D1::RectF(
    static_cast<FLOAT>(dstRect->left),
    static_cast<FLOAT>(dstRect->top),
    static_cast<FLOAT>(dstRect->right),
    static_cast<FLOAT>(dstRect->bottom)
  );
  
  // Ensure destination rectangle is within render target bounds
  if (d2dDstRect.right > rtSize.width) {
    LOG_D2D("Destination right edge exceeds render target width, adjusting from %.2f to %.2f", 
            d2dDstRect.right, rtSize.width);
    d2dDstRect.right = rtSize.width;
  }
  
  if (d2dDstRect.bottom > rtSize.height) {
    LOG_D2D("Destination bottom edge exceeds render target height, adjusting from %.2f to %.2f", 
            d2dDstRect.bottom, rtSize.height);
    d2dDstRect.bottom = rtSize.height;
  }

  // Source rectangle from the bitmap - this handles the resizing
  D2D1_RECT_F d2dSrcRect = D2D1::RectF(
    static_cast<FLOAT>(srcRect->left),
    static_cast<FLOAT>(srcRect->top),
    static_cast<FLOAT>(srcRect->right),
    static_cast<FLOAT>(srcRect->bottom)
  );

  // Calculate scale for logging
  float scaleX = (d2dDstRect.right - d2dDstRect.left) / (d2dSrcRect.right - d2dSrcRect.left);
  float scaleY = (d2dDstRect.bottom - d2dDstRect.top) / (d2dSrcRect.bottom - d2dSrcRect.top);
  LOG_D2D("Scale factors: %.2f x %.2f", scaleX, scaleY);

  LOG_D2D("Drawing bitmap - source: (%.2f,%.2f,%.2f,%.2f), dest: (%.2f,%.2f,%.2f,%.2f)",
          d2dSrcRect.left, d2dSrcRect.top, d2dSrcRect.right, d2dSrcRect.bottom,
          d2dDstRect.left, d2dDstRect.top, d2dDstRect.right, d2dDstRect.bottom);

  // Use NEAREST_NEIGHBOR for exact pixel mapping
  m_pRenderTarget->DrawBitmap(
    m_pBitmap,
    d2dDstRect,     // Destination rectangle (window size)
    1.0f,           // Opacity
    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, // Use nearest neighbor for exact pixel mapping
    d2dSrcRect      // Source rectangle from the bitmap
  );

  hr = m_pRenderTarget->EndDraw();
  if (FAILED(hr)) {
    LOG_D2D("EndDraw failed with error: 0x%08x", hr);
    return;
  }

  LOG_D2D("stretchFromDibSection completed successfully");
}

void Direct2DSection::blitToDibSection(const Rect *rect, DWORD flags)
{
  throw SystemException(_T("blitToDibSection is not supported in the Direct2D implementation"));
}

void Direct2DSection::initDirect2D(const PixelFormat *pf, const Dimension *dim, HWND compatibleWin)
{
  DEBUG_LOG("initDirect2D called for window %p, dimensions: %dx%d", compatibleWin, dim->width, dim->height);
  
  // For prototype: If compatibleWin is NULL, fallback to using the entire desktop instead of throwing
  if (compatibleWin == 0) {
    DEBUG_LOG("Compatible window was NULL, using desktop window instead");
    compatibleWin = GetDesktopWindow();
    if (compatibleWin == 0) {
      DEBUG_LOG("Failed to get desktop window");
      throw SystemException(_T("Failed to get desktop window for Direct2D initialization"));
    }
  }

  // Verify the window is valid
  if (!IsWindow(compatibleWin)) {
    DEBUG_LOG("Invalid window handle: %p", compatibleWin);
    throw SystemException(_T("Invalid window handle for Direct2D initialization"));
  }
  m_hwnd = compatibleWin;

  // Create Direct2D factory
  HRESULT hr = CreateD2DFactory(&m_pD2DFactory);
  if (FAILED(hr)) {
    DEBUG_LOG("Failed to create Direct2D factory, HRESULT: 0x%lx", hr);
    throw SystemException(_T("Failed to create Direct2D factory"));
  }
  DEBUG_LOG("Direct2D factory created successfully");

  // Create render target with Windows 7 compatible settings
  // Make sure dimensions are at least 1x1 to avoid D2D errors
  UINT width = std::max(1, dim->width);
  UINT height = std::max(1, dim->height);
  
  // Get actual window client size for better rendering
  RECT clientRect;
  GetClientRect(compatibleWin, &clientRect);
  UINT winWidth = clientRect.right - clientRect.left;
  UINT winHeight = clientRect.bottom - clientRect.top;
  
  D2D1_SIZE_U renderSize;
  if (winWidth > 0 && winHeight > 0) {
    DEBUG_LOG("Using actual window client size: %dx%d", winWidth, winHeight);
    renderSize = D2D1::SizeU(winWidth, winHeight);
  } else {
    renderSize = D2D1::SizeU(width, height);
  }

  // Use specific settings that work well with Windows 7
  D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
    D2D1_RENDER_TARGET_TYPE_DEFAULT,
    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
    0.0f, // Default DPI
    0.0f, // Default DPI
    D2D1_RENDER_TARGET_USAGE_NONE, // No specific usage flags
    D2D1_FEATURE_LEVEL_DEFAULT     // Use default feature level (Direct2D 1.0 in Windows 7)
  );
  
  // Use D2D1_PRESENT_OPTIONS_NONE for better compatibility
  D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
    compatibleWin,
    renderSize,
    D2D1_PRESENT_OPTIONS_NONE  // Changed from IMMEDIATELY to NONE for better rendering
  );

  DEBUG_LOG("Attempting to create HwndRenderTarget for window %p with size %ux%u", compatibleWin, renderSize.width, renderSize.height);
  
  // Try to create the render target (as ID2D1RenderTarget interface)
  ID2D1HwndRenderTarget* pHwndRT = NULL;
  hr = m_pD2DFactory->CreateHwndRenderTarget(
    rtProps,
    hwndProps,
    &pHwndRT
  );

  if (SUCCEEDED(hr) && pHwndRT) {
    DEBUG_LOG("HwndRenderTarget created successfully");
    m_pRenderTarget = pHwndRT;
    m_pHwndRenderTarget = pHwndRT;
  } else {
    DEBUG_LOG("Failed to create HwndRenderTarget, HRESULT: 0x%lx, trying DCRenderTarget instead", hr);
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
      DEBUG_LOG("DCRenderTarget created successfully");
      // If we created a DC render target successfully, bind it to the window DC temporarily
      HDC hdc = GetDC(compatibleWin);
      if (hdc) {
        RECT rc = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
        if (winWidth > 0 && winHeight > 0) {
          rc.right = clientRect.right;
          rc.bottom = clientRect.bottom;
        }
        
        hr = pDCRT->BindDC(hdc, &rc);
        DEBUG_LOG("BindDC result: 0x%lx, rect: (%d,%d,%d,%d)", 
                 hr, rc.left, rc.top, rc.right, rc.bottom);
        ReleaseDC(compatibleWin, hdc);
      } else {
        DEBUG_LOG("Failed to get HDC for window %p", compatibleWin);
      }
      
      m_pRenderTarget = pDCRT;
      m_pDCRenderTarget = pDCRT;
    } else {
      DEBUG_LOG("Failed to create DCRenderTarget, HRESULT: 0x%lx", hr);
      throw SystemException(_T("Failed to create any compatible Direct2D render target"));
    }
  }

  // Create bitmap format - Use DXGI_FORMAT_B8G8R8A8_UNORM which is supported in Windows 7
  D2D1_PIXEL_FORMAT pixelFormat = D2D1::PixelFormat(
    DXGI_FORMAT_B8G8R8A8_UNORM,  // Windows 7 compatible format
    D2D1_ALPHA_MODE_IGNORE       // Ignore alpha
  );

  // Create bitmap with the same size as dim (not the window size)
  D2D1_SIZE_U bitmapSize = D2D1::SizeU(width, height);
  D2D1_BITMAP_PROPERTIES bitmapProps = D2D1::BitmapProperties(pixelFormat);
  
  // Create an empty bitmap initially
  hr = m_pRenderTarget->CreateBitmap(
    bitmapSize,
    NULL,
    0,
    bitmapProps,
    &m_pBitmap
  );

  if (FAILED(hr)) {
    DEBUG_LOG("Failed to create Direct2D bitmap, HRESULT: 0x%lx", hr);
    throw SystemException(_T("Failed to create Direct2D bitmap"));
  }
  DEBUG_LOG("Direct2D bitmap created successfully");

  // Allocate memory for bitmap data
  m_bitmapBits = malloc(width * height * 4);
  if (m_bitmapBits == NULL) {
    DEBUG_LOG("Failed to allocate bitmap data buffer");
    throw SystemException(_T("Failed to allocate bitmap data buffer"));
  }
  
  // Initialize the bitmap buffer with black
  memset(m_bitmapBits, 0, width * height * 4);
  
  // Draw a test pattern to the Direct2D render target to make sure it works
  m_pRenderTarget->BeginDraw();
  m_pRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f)); // Black
  HRESULT testResult = m_pRenderTarget->EndDraw();
  DEBUG_LOG("Test pattern draw result: 0x%lx", testResult);
}

/**
 * Clean up all Direct2D resources
 */
void Direct2DSection::releaseDirect2D()
{
  DEBUG_LOG("releaseDirect2D called");
  
  // Free bitmap data buffer
  if (m_bitmapBits) {
    free(m_bitmapBits);
    m_bitmapBits = NULL;
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
    m_pDCRenderTarget = NULL;
    m_pHwndRenderTarget = NULL;
  }
  
  if (m_pD2DFactory) {
    m_pD2DFactory->Release();
    m_pD2DFactory = NULL;
  }
  DEBUG_LOG("All Direct2D resources released");
}

void Direct2DSection::drawTestPattern()
{
  DEBUG_LOG("drawTestPattern called");
  
  if (!m_pRenderTarget || !m_pBitmap || !m_bitmapBits) {
    DEBUG_LOG("ERROR: Render target, bitmap, or buffer is NULL!");
    return;
  }
  
  // Draw a test pattern directly to the bitmap buffer
  BYTE* buffer = (BYTE*)m_bitmapBits;
  int width = m_width;
  int height = m_height;
  
  // Fill with a simple gradient pattern
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      BYTE r = (BYTE)((float)x / width * 255);
      BYTE g = (BYTE)((float)y / height * 255);
      BYTE b = 128;
      BYTE a = 255;
      
      int offset = (y * width + x) * 4;
      buffer[offset] = b;     // Blue
      buffer[offset + 1] = g; // Green
      buffer[offset + 2] = r; // Red
      buffer[offset + 3] = a; // Alpha
    }
  }
  
  // Draw some test lines
  for (int i = 0; i < width && i < height; i++) {
    int offset = (i * width + i) * 4;
    buffer[offset] = 255;     // Blue
    buffer[offset + 1] = 255; // Green
    buffer[offset + 2] = 255; // Red
    buffer[offset + 3] = 255; // Alpha
  }
  
  // Update the Direct2D bitmap with the test pattern
  m_pBitmap->CopyFromMemory(NULL, m_bitmapBits, width * 4);
  
  // Draw the bitmap to the render target
  m_pRenderTarget->BeginDraw();
  m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
  
  D2D1_SIZE_F rtSize = m_pRenderTarget->GetSize();
  D2D1_RECT_F dstRect = D2D1::RectF(0, 0, rtSize.width, rtSize.height);
  
  m_pRenderTarget->DrawBitmap(
    m_pBitmap,
    dstRect,
    1.0f,
    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
  );
  
  HRESULT hr = m_pRenderTarget->EndDraw();
  if (FAILED(hr)) {
    DEBUG_LOG("EndDraw failed with error: 0x%08x", hr);
  }
}

void Direct2DSection::drawDirectTestPattern(const Rect *rect)
{
  // Draw directly to the render target using Direct2D primitives
  if (!m_pRenderTarget) {
    DEBUG_LOG("ERROR: Render target is NULL!");
    return;
  }
  
  m_pRenderTarget->BeginDraw();
  m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White, 0.5f));
  
  // Create brushes for drawing
  ID2D1SolidColorBrush* pRedBrush = NULL;
  ID2D1SolidColorBrush* pGreenBrush = NULL;
  ID2D1SolidColorBrush* pBlueBrush = NULL;
  
  HRESULT hr = m_pRenderTarget->CreateSolidColorBrush(
    D2D1::ColorF(D2D1::ColorF::Red), &pRedBrush);
  
  if (SUCCEEDED(hr)) {
    hr = m_pRenderTarget->CreateSolidColorBrush(
      D2D1::ColorF(D2D1::ColorF::Green), &pGreenBrush);
  }
  
  if (SUCCEEDED(hr)) {
    hr = m_pRenderTarget->CreateSolidColorBrush(
      D2D1::ColorF(D2D1::ColorF::Blue), &pBlueBrush);
  }
  
  if (SUCCEEDED(hr)) {
    // Get render target size
    D2D1_SIZE_F rtSize = m_pRenderTarget->GetSize();
    
    // Draw a rectangle
    D2D1_RECT_F rectangle = D2D1::RectF(
      rtSize.width / 4,
      rtSize.height / 4,
      rtSize.width * 3 / 4,
      rtSize.height * 3 / 4
    );
    
    m_pRenderTarget->DrawRectangle(rectangle, pRedBrush, 2.0f);
    
    // Draw an ellipse
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(
      D2D1::Point2F(rtSize.width / 2, rtSize.height / 2),
      rtSize.width / 4,
      rtSize.height / 4
    );
    
    m_pRenderTarget->DrawEllipse(ellipse, pGreenBrush, 2.0f);
    
    // Draw a line
    m_pRenderTarget->DrawLine(
      D2D1::Point2F(0, 0),
      D2D1::Point2F(rtSize.width, rtSize.height),
      pBlueBrush,
      2.0f
    );
    
    // Release brushes
    pRedBrush->Release();
    pGreenBrush->Release();
    pBlueBrush->Release();
  }
  
  hr = m_pRenderTarget->EndDraw();
  if (FAILED(hr)) {
    DEBUG_LOG("EndDraw failed with error: 0x%08x", hr);
  }
}

void Direct2DSection::drawCrosshair(int x, int y, COLORREF color)
{
  if (!m_pRenderTarget) {
    LOG_D2D("Cannot draw crosshair - render target is NULL");
    return;
  }
  
  // Extract RGB components
  FLOAT r = GetRValue(color) / 255.0f;
  FLOAT g = GetGValue(color) / 255.0f;
  FLOAT b = GetBValue(color) / 255.0f;
  
  // Create a solid color brush
  ID2D1SolidColorBrush* pBrush = NULL;
  HRESULT hr = m_pRenderTarget->CreateSolidColorBrush(
    D2D1::ColorF(r, g, b, 1.0f),
    &pBrush
  );
  
  if (SUCCEEDED(hr) && pBrush) {
    // Draw crosshair
    const float length = 10.0f;
    const float width = 1.0f;
    
    D2D1_POINT_2F center = D2D1::Point2F(static_cast<FLOAT>(x), static_cast<FLOAT>(y));
    
    // Horizontal line
    m_pRenderTarget->DrawLine(
      D2D1::Point2F(center.x - length, center.y),
      D2D1::Point2F(center.x + length, center.y),
      pBrush,
      width
    );
    
    // Vertical line
    m_pRenderTarget->DrawLine(
      D2D1::Point2F(center.x, center.y - length),
      D2D1::Point2F(center.x, center.y + length),
      pBrush,
      width
    );
    
    // Draw a small circle at the center
    D2D1_ELLIPSE centerDot = D2D1::Ellipse(center, 2.0f, 2.0f);
    m_pRenderTarget->FillEllipse(centerDot, pBrush);
    
    pBrush->Release();
  } else {
    LOG_D2D("Failed to create brush for crosshair, error: 0x%08x", hr);
  }
}

//bool Direct2DSection::UpdateBitmapFromDIB(ID2D1Bitmap* pBitmap, const Rect* rect, void* dibBits, UINT dibStride)
//{
//  if (!pBitmap || !rect || !dibBits) {
//    LOG_D2D("UpdateBitmapFromDIB: Invalid parameters");
//    return false;
//  }
//  
//  // Get bitmap size to ensure we don't exceed bounds
//  D2D1_SIZE_U bitmapSize = pBitmap->GetPixelSize();
//  
//  // Calculate the source pointer within the DIB
//  BYTE* srcBits = static_cast<BYTE*>(dibBits);
//  srcBits += rect->top * dibStride + rect->left * 4;  // Assuming 32bpp
//  
//  // Create the update rectangle for Direct2D
//  D2D1_RECT_U updateRect = D2D1::RectU(
//    0,  // Start at left edge of bitmap
//    0,  // Start at top edge of bitmap
//    rect->right - rect->left,  // Width of the area to update
//    rect->bottom - rect->top   // Height of the area to update
//  );
//  
//  // Update the bitmap with the specified region data
//  HRESULT hr = pBitmap->CopyFromMemory(&updateRect, srcBits, dibStride);
//  if (FAILED(hr)) {
//    LOG_D2D("CopyFromMemory failed with HRESULT: 0x%lx", hr);
//    return false;
//  }
//  
//  return true;
//}

void Direct2DSection::resize(const Rect* newSize) {
  RECT rect = newSize->toWindowsRect();

  UINT newWidth = rect.right - rect.left;
  UINT newHeight = rect.bottom - rect.top;

  HRESULT hr = S_OK;

  if (newWidth != m_pRenderTarget->GetPixelSize().width ||
      newHeight != m_pRenderTarget->GetPixelSize().height) {
    if (m_pHwndRenderTarget) {
      DEBUG_LOG("hwnd need resize");
      hr = m_pHwndRenderTarget->Resize(D2D1::SizeU(newWidth, newHeight));
      if (FAILED(hr)) {
        DEBUG_LOG("hwnd resize failed: 0x%lx", hr);
      }
    }
    else if (m_pDCRenderTarget) {
      DEBUG_LOG("DC need rebind");
      
      HDC hdc = GetDC(m_hwnd);
      if (hdc) {
        HRESULT hr = m_pDCRenderTarget->BindDC(hdc, &rect);
        DEBUG_LOG("Rebind DC result: 0x%lx, rect: (%d,%d,%d,%d)",
          hr, rect.left, rect.top, rect.right, rect.bottom);
        ReleaseDC(m_hwnd, hdc);
      }
      else {
        DEBUG_LOG("Rebind: Failed to get HDC for window %p", m_hwnd);
      }
    }
  }
}