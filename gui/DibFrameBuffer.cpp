// Copyright (C) 2012 GlavSoft LLC.
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

#include "DibFrameBuffer.h"
#include "util/Exception.h"

// Debug logging
#ifdef _DEBUG
#define DEBUG_LOG(tag, msg, ...) { FILE* f = fopen("d2d_debug.log", "a"); if(f) { fprintf(f, "[%s] " msg "\n", tag, ##__VA_ARGS__); fclose(f); } }
#define LOG_DIBFB(msg, ...) DEBUG_LOG("DIBFB", msg, ##__VA_ARGS__)
#else
#define DEBUG_LOG(tag, msg, ...)
#define LOG_DIBFB(msg, ...)
#endif

DibFrameBuffer::DibFrameBuffer()
: m_renderManager(0)
{
}

DibFrameBuffer::~DibFrameBuffer()
{
  releaseRenderManager();
}

void DibFrameBuffer::setTargetDC(HDC targetDC)
{
  // This method is no longer needed with RenderManager, but kept for compatibility
  // The RenderManager handles its own target DC
  checkRenderManagerValid();
}

bool DibFrameBuffer::assignProperties(const FrameBuffer *srcFrameBuffer)
{
  throw Exception(_T("Wrong: You shouln't use the DibFrameBuffer::assignProperties() function."));
}

bool DibFrameBuffer::clone(const FrameBuffer *srcFrameBuffer)
{
  throw Exception(_T("Wrong: You shouln't use the DibFrameBuffer::clone() function."));
}

void DibFrameBuffer::setColor(UINT8 reg, UINT8 green, UINT8 blue)
{
  m_fb.setColor(reg, green, blue);
}

void DibFrameBuffer::fillRect(const Rect *dstRect, UINT32 color)
{
  m_fb.fillRect(dstRect, color);
}

bool DibFrameBuffer::isEqualTo(const FrameBuffer *frameBuffer)
{
  return m_fb.isEqualTo(frameBuffer);
}

bool DibFrameBuffer::copyFrom(const Rect *dstRect, const FrameBuffer *srcFrameBuffer,
                              int srcX, int srcY)
{
  return m_fb.copyFrom(dstRect, srcFrameBuffer, srcX, srcY);
}

bool DibFrameBuffer::copyFrom(const FrameBuffer *srcFrameBuffer, int srcX, int srcY)
{
  return m_fb.copyFrom(srcFrameBuffer, srcX, srcY);
}

bool DibFrameBuffer::overlay(const Rect *dstRect, const FrameBuffer *srcFrameBuffer,
                             int srcX, int srcY, const char *andMask)
{
  return m_fb.overlay(dstRect, srcFrameBuffer, srcX, srcY, andMask);
}

void DibFrameBuffer::move(const Rect *dstRect, const int srcX, const int srcY)
{
  m_fb.move(dstRect, srcX, srcY);
}

bool DibFrameBuffer::cmpFrom(const Rect *dstRect, const FrameBuffer *srcFrameBuffer,
                             const int srcX, const int srcY)
{
  return m_fb.cmpFrom(dstRect, srcFrameBuffer, srcX, srcY);
}

bool DibFrameBuffer::setDimension(const Dimension *newDim)
{
  throw Exception(_T("Wrong: You shouln't use the DibFrameBuffer::clone() function."));
}

bool DibFrameBuffer::setDimension(const Rect *rect)
{
  throw Exception(_T("Wrong: You shouln't use the DibFrameBuffer::clone() function."));
}

void DibFrameBuffer::setEmptyDimension(const Rect *dimByRect)
{
  throw Exception(_T("This function is deprecated"));
}

void DibFrameBuffer::setEmptyPixelFmt(const PixelFormat *pf)
{
  throw Exception(_T("This function is deprecated"));
}

void DibFrameBuffer::setPropertiesWithoutResize(const Dimension *newDim, const PixelFormat *pf)
{
  throw Exception(_T("Wrong: You shouln't use the DibFrameBuffer::setPropertiesWithoutResize() function."));
}

inline Dimension DibFrameBuffer::getDimension() const
{
  return m_fb.getDimension();
}

bool DibFrameBuffer::setPixelFormat(const PixelFormat *pixelFormat)
{
  throw Exception(_T("Wrong: You shouln't use the DibFrameBuffer::setPixelFormat() function."));
}

inline PixelFormat DibFrameBuffer::getPixelFormat() const
{
  return m_fb.getPixelFormat();
}

bool DibFrameBuffer::setProperties(const Dimension *newDim, const PixelFormat *pixelFormat)
{
  throw Exception(_T("Wrong: You shouln't use this variant of the DibFrameBuffer::setProperties() function."));
}

bool DibFrameBuffer::setProperties(const Rect *dimByRect, const PixelFormat *pixelFormat)
{
  throw Exception(_T("Wrong: You shouln't use this variant of the DibFrameBuffer::setProperties() function."));
}

UINT8 DibFrameBuffer::getBitsPerPixel() const
{
  return m_fb.getBitsPerPixel();
}

UINT8 DibFrameBuffer::getBytesPerPixel() const
{
  return m_fb.getBytesPerPixel();
}

void DibFrameBuffer::setBuffer(void *newBuffer)
{
  throw Exception(_T("Wrong: You shouln't use the DibFrameBuffer::setBuffer() function."));
}

inline void *DibFrameBuffer::getBuffer() const
{
  return m_fb.getBuffer();
}

void *DibFrameBuffer::getBufferPtr(int x, int y) const
{
  return m_fb.getBufferPtr(x, y);
}

inline int DibFrameBuffer::getBufferSize() const
{
  return m_fb.getBufferSize();
}

inline int DibFrameBuffer::getBytesPerRow() const
{
  return m_fb.getBytesPerRow();
}

void DibFrameBuffer::blitToDibSection(const Rect *rect)
{
  checkRenderManagerValid();
  m_renderManager->blitToDibSection(rect);
}

void DibFrameBuffer::blitTransparentToDibSection(const Rect *rect)
{
  checkRenderManagerValid();
  m_renderManager->blitTransparentToDibSection(rect);
}

void DibFrameBuffer::blitFromDibSection(const Rect *rect)
{
  checkRenderManagerValid();
  m_renderManager->blitFromDibSection(rect);
}

void DibFrameBuffer::stretchFromDibSection(const Rect *srcRect, const Rect *dstRect)
{
  checkRenderManagerValid();
  m_renderManager->stretchFromDibSection(srcRect, dstRect);
}

bool DibFrameBuffer::setRenderMode(RenderMode mode)
{
  checkRenderManagerValid();
  bool status = m_renderManager->setRenderMode(mode);
  if (status) {
    m_fb.setBuffer(m_renderManager->getBuffer());
    return true;
  }
  return false;
}

RenderMode DibFrameBuffer::getRenderMode() const
{
  // this method might be called before render manager is even initialized
  if (m_renderManager == 0) {
    return RENDER_MODE_GDI;
  }
  return m_renderManager->getRenderMode();
}

void DibFrameBuffer::setProperties(const Dimension *newDim,
                                   const PixelFormat *pixelFormat,
                                   HWND compatibleWindow)
{
  m_fb.setPropertiesWithoutResize(newDim, pixelFormat);
  void *buffer = updateRenderManager(newDim, pixelFormat, compatibleWindow);
  m_fb.setBuffer(buffer);
}

void *DibFrameBuffer::updateRenderManager(const Dimension *newDim,
                                      const PixelFormat *pixelFormat,
                                      HWND compatibleWindow)
{
  // Add debug logging
  LOG_DIBFB("updateRenderManager: incoming window handle: %p", compatibleWindow);

  releaseRenderManager();
  
  // Check if the window handle is valid - if not, try using desktop window
  if (compatibleWindow == NULL || !IsWindow(compatibleWindow)) {
    LOG_DIBFB("Invalid window handle, falling back to desktop window");
    compatibleWindow = GetDesktopWindow();
  }
  
  // Start with GDI rendering by default for maximum compatibility
  RenderMode initialMode = RENDER_MODE_GDI;
  
  LOG_DIBFB("Creating RenderManager with window handle: %p, dimensions: %dx%d", 
            compatibleWindow, newDim->width, newDim->height);
  
  try {
    // Create new RenderManager with default GDI rendering
    m_renderManager = new RenderManager(pixelFormat, newDim, compatibleWindow, initialMode);
    void* buffer = m_renderManager->getBuffer();
    
    LOG_DIBFB("RenderManager created successfully, buffer: %p", buffer);
    
    return buffer;
  } catch (Exception &e) {
    // If we couldn't create the render manager, clean up and rethrow
    LOG_DIBFB("Exception creating RenderManager: %s", e.getMessage());
    
    releaseRenderManager();
    throw;
  }
}

void DibFrameBuffer::releaseRenderManager()
{
  if (m_renderManager) {
    m_fb.setBuffer(0);
    delete m_renderManager;
    m_renderManager = 0;
  }
}

void DibFrameBuffer::checkRenderManagerValid() const
{
  if (m_renderManager == 0) {
    throw Exception(_T("Can't perform operation because RenderManager is not initialized yet"));
  }
}

void DibFrameBuffer::drawTestPattern()
{
  LOG_DIBFB("drawTestPattern called");
  
  try {
    // Check if the render manager is valid
    checkRenderManagerValid();
    
    // Forward the call to the render manager
    m_renderManager->drawTestPattern();
    
    LOG_DIBFB("drawTestPattern completed successfully");
  } catch (std::exception& e) {
    LOG_DIBFB("Exception in drawTestPattern: %s", e.what());
  } catch (...) {
    LOG_DIBFB("Unknown exception in drawTestPattern");
  }
}

void DibFrameBuffer::resize(const Rect* newSize) {
  checkRenderManagerValid();
  m_renderManager->resize(newSize);
}