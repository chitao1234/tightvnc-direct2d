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

#include "RenderManager.h"
#include "win-system/SystemException.h"
#include <d2d1.h>

/**
 * RenderManager constructor
 * 
 * Creates a renderer with the specified mode (GDI or Direct2D)
 * If Direct2D is requested but not available, it will fall back to GDI mode
 */
RenderManager::RenderManager(const PixelFormat *pf, const Dimension *dim, 
                            HWND compatibleWin, RenderMode mode)
  : m_mode(mode),
    m_dibSection(NULL),
    m_direct2DSection(NULL),
    m_pixelFormat(*pf),
    m_dimension(*dim),
    m_window(compatibleWin)
{
  // Try to use Direct2D if requested
  if (m_mode == RENDER_MODE_DIRECT2D && !isD2DAvailable()) {
    // Fall back to GDI if Direct2D is not available
    m_mode = RENDER_MODE_GDI;
  }

  // Create appropriate renderer
  createRenderer();
}

RenderManager::~RenderManager()
{
  destroyRenderer();
}

void *RenderManager::getBuffer()
{
  if (m_mode == RENDER_MODE_DIRECT2D && m_direct2DSection) {
    return m_direct2DSection->getBuffer();
  } else if (m_dibSection) {
    return m_dibSection->getBuffer();
  }
  return NULL;
}

void RenderManager::blitToDibSection(const Rect *rect)
{
  if (m_mode == RENDER_MODE_DIRECT2D && m_direct2DSection) {
    m_direct2DSection->blitToDibSection(rect);
  } else if (m_dibSection) {
    m_dibSection->blitToDibSection(rect);
  }
}

void RenderManager::blitTransparentToDibSection(const Rect *rect)
{
  if (m_mode == RENDER_MODE_DIRECT2D && m_direct2DSection) {
    m_direct2DSection->blitTransparentToDibSection(rect);
  } else if (m_dibSection) {
    m_dibSection->blitTransparentToDibSection(rect);
  }
}

void RenderManager::blitFromDibSection(const Rect *rect)
{
  if (m_mode == RENDER_MODE_DIRECT2D && m_direct2DSection) {
    m_direct2DSection->blitFromDibSection(rect);
  } else if (m_dibSection) {
    m_dibSection->blitFromDibSection(rect);
  }
}

void RenderManager::stretchFromDibSection(const Rect *srcRect, const Rect *dstRect)
{
  if (m_mode == RENDER_MODE_DIRECT2D && m_direct2DSection) {
    m_direct2DSection->stretchFromDibSection(srcRect, dstRect);
  } else if (m_dibSection) {
    m_dibSection->stretchFromDibSection(srcRect, dstRect);
  }
}

/**
 * Changes the rendering mode between GDI and Direct2D
 * 
 * This destroys the current renderer and creates a new one with the specified mode.
 * If Direct2D is requested but not available, the function will return false and
 * keep using the current renderer.
 * 
 * @param mode The desired rendering mode (GDI or Direct2D)
 * @return true if mode was successfully set, false otherwise
 */
bool RenderManager::setRenderMode(RenderMode mode)
{
  // If trying to set Direct2D but it's not available, return false
  if (mode == RENDER_MODE_DIRECT2D && !isD2DAvailable()) {
    return false;
  }

  // If mode hasn't changed, do nothing
  if (mode == m_mode) {
    return true;
  }

  // Destroy current renderer
  destroyRenderer();

  // Set new mode
  m_mode = mode;

  // Create new renderer
  createRenderer();

  return true;
}

/**
 * Creates the appropriate renderer (GDI or Direct2D) based on the current mode
 * 
 * If Direct2D creation fails, it will fall back to GDI mode automatically
 */
void RenderManager::createRenderer()
{
  try {
    if (m_mode == RENDER_MODE_DIRECT2D) {
      // Make sure we have a valid window
      if (m_window == NULL || !IsWindow(m_window)) {
        // Use desktop window as fallback if window handle is invalid
        m_window = GetDesktopWindow();
      }
      
      // Only try to create Direct2D section if we have a valid window now
      if (m_window != NULL && IsWindow(m_window)) {
        m_direct2DSection = new Direct2DSection(&m_pixelFormat, &m_dimension, m_window);
      } else {
        // Fallback to GDI if no valid window
        m_mode = RENDER_MODE_GDI;
        m_dibSection = new DibSection(&m_pixelFormat, &m_dimension, m_window);
      }
    } else {
      m_dibSection = new DibSection(&m_pixelFormat, &m_dimension, m_window);
    }
  } catch (SystemException &e) {
    // If Direct2D creation failed, fall back to GDI
    if (m_mode == RENDER_MODE_DIRECT2D) {
      m_mode = RENDER_MODE_GDI;
      m_direct2DSection = NULL;
      m_dibSection = new DibSection(&m_pixelFormat, &m_dimension, m_window);
    } else {
      throw; // Re-throw for GDI failures
    }
  } catch (...) {
    // Catch any other exceptions and fallback to GDI
    if (m_mode == RENDER_MODE_DIRECT2D) {
      m_mode = RENDER_MODE_GDI;
      m_direct2DSection = NULL;
      try {
        m_dibSection = new DibSection(&m_pixelFormat, &m_dimension, m_window);
      } catch (...) {
        // If even GDI fails, we have a serious problem
        throw SystemException(_T("Failed to create any renderer"));
      }
    } else {
      throw; // Re-throw for GDI failures
    }
  }
}

/**
 * Destroys the current renderer and releases all resources
 */
void RenderManager::destroyRenderer()
{
  if (m_direct2DSection) {
    delete m_direct2DSection;
    m_direct2DSection = NULL;
  }

  if (m_dibSection) {
    delete m_dibSection;
    m_dibSection = NULL;
  }
}

/**
 * Checks if Direct2D is available on this system
 * 
 * This tries to create a Direct2D factory which will fail if Direct2D
 * is not supported or available on the system.
 * 
 * @return true if Direct2D is available, false otherwise
 */
bool RenderManager::isD2DAvailable()
{
  // Check if Direct2D 1.0 (Windows 7) is available on this system
  ID2D1Factory* factory = NULL;
  
  // Use D2D1CreateFactory which is available in Windows 7 SDK
  HRESULT hr = D2D1CreateFactory(
    D2D1_FACTORY_TYPE_SINGLE_THREADED,
    __uuidof(ID2D1Factory),  // Use __uuidof to get the IID for Windows 7 compat
    NULL,                    // No special creation options needed for Windows 7
    reinterpret_cast<void**>(&factory)
  );
  
  if (SUCCEEDED(hr) && factory) {
    // Successfully created a Direct2D 1.0 factory
    factory->Release();
    return true;
  }
  
  return false;
} 