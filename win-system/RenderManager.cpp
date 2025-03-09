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
#include <stdio.h>

// Debug logging - use the same macro as Direct2DSection
// Debug logging
#ifdef _DEBUG
#define DEBUG_LOG(msg, ...) { FILE* f = fopen("d2d_debug.log", "a"); if(f) { fprintf(f, "[D2D] " msg "\n", ##__VA_ARGS__); fclose(f); } }
#else
#define DEBUG_LOG(msg, ...)
#endif

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
  DEBUG_LOG("RenderManager constructor - mode: %s, window: %p, dimensions: %dx%d", 
           (mode == RENDER_MODE_DIRECT2D ? "Direct2D" : "GDI"), 
           compatibleWin, dim->width, dim->height);
  
  // Try to use Direct2D if requested
  if (m_mode == RENDER_MODE_DIRECT2D && !isD2DAvailable()) {
    DEBUG_LOG("Direct2D requested but not available, falling back to GDI");
    // Fall back to GDI if Direct2D is not available
    m_mode = RENDER_MODE_GDI;
  }

  // Create appropriate renderer
  createRenderer();
  DEBUG_LOG("RenderManager constructor complete - final mode: %s", 
           (m_mode == RENDER_MODE_DIRECT2D ? "Direct2D" : "GDI"));
}

RenderManager::~RenderManager()
{
  DEBUG_LOG("RenderManager destructor");
  destroyRenderer();
}

void *RenderManager::getBuffer()
{
  void* buffer = NULL;
  if (m_mode == RENDER_MODE_DIRECT2D && m_direct2DSection) {
    buffer = m_direct2DSection->getBuffer();
  } else if (m_dibSection) {
    buffer = m_dibSection->getBuffer();
  }
  DEBUG_LOG("getBuffer returning %p", buffer);
  return buffer;
}

void RenderManager::blitToDibSection(const Rect *rect)
{
  DEBUG_LOG("blitToDibSection: rect=(%d,%d,%d,%d), mode=%s", 
           rect->left, rect->top, rect->right, rect->bottom, 
           (m_mode == RENDER_MODE_DIRECT2D ? "Direct2D" : "GDI"));
  
  if (m_mode == RENDER_MODE_DIRECT2D && m_direct2DSection) {
    m_direct2DSection->blitToDibSection(rect);
  } else if (m_dibSection) {
    m_dibSection->blitToDibSection(rect);
  } else {
    DEBUG_LOG("ERROR: No renderer available for blitToDibSection!");
  }
}

void RenderManager::blitTransparentToDibSection(const Rect *rect)
{
  DEBUG_LOG("blitTransparentToDibSection: rect=(%d,%d,%d,%d), mode=%s", 
           rect->left, rect->top, rect->right, rect->bottom, 
           (m_mode == RENDER_MODE_DIRECT2D ? "Direct2D" : "GDI"));
  
  if (m_mode == RENDER_MODE_DIRECT2D && m_direct2DSection) {
    m_direct2DSection->blitTransparentToDibSection(rect);
  } else if (m_dibSection) {
    m_dibSection->blitTransparentToDibSection(rect);
  } else {
    DEBUG_LOG("ERROR: No renderer available for blitTransparentToDibSection!");
  }
}

void RenderManager::blitFromDibSection(const Rect *rect)
{
  DEBUG_LOG("blitFromDibSection: rect=(%d,%d,%d,%d), mode=%s", 
           rect->left, rect->top, rect->right, rect->bottom, 
           (m_mode == RENDER_MODE_DIRECT2D ? "Direct2D" : "GDI"));
  
  if (m_mode == RENDER_MODE_DIRECT2D && m_direct2DSection) {
    m_direct2DSection->blitFromDibSection(rect);
  } else if (m_dibSection) {
    m_dibSection->blitFromDibSection(rect);
  } else {
    DEBUG_LOG("ERROR: No renderer available for blitFromDibSection!");
  }
}

void RenderManager::stretchFromDibSection(const Rect *srcRect, const Rect *dstRect)
{
  DEBUG_LOG("stretchFromDibSection: src=(%d,%d,%d,%d), dst=(%d,%d,%d,%d), mode=%s", 
           srcRect->left, srcRect->top, srcRect->right, srcRect->bottom,
           dstRect->left, dstRect->top, dstRect->right, dstRect->bottom,
           (m_mode == RENDER_MODE_DIRECT2D ? "Direct2D" : "GDI"));
  
  if (m_mode == RENDER_MODE_DIRECT2D && m_direct2DSection) {
    m_direct2DSection->stretchFromDibSection(srcRect, dstRect);
  } else if (m_dibSection) {
    m_dibSection->stretchFromDibSection(srcRect, dstRect);
  } else {
    DEBUG_LOG("ERROR: No renderer available for stretchFromDibSection!");
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
  DEBUG_LOG("setRenderMode: requested mode=%s, current mode=%s", 
           (mode == RENDER_MODE_DIRECT2D ? "Direct2D" : "GDI"),
           (m_mode == RENDER_MODE_DIRECT2D ? "Direct2D" : "GDI"));
  
  // If trying to set Direct2D but it's not available, return false
  if (mode == RENDER_MODE_DIRECT2D && !isD2DAvailable()) {
    DEBUG_LOG("Direct2D requested but not available, keeping current mode");
    return false;
  }

  // If mode hasn't changed, do nothing
  if (mode == m_mode) {
    DEBUG_LOG("Requested mode is same as current mode, no change needed");
    return false;
  }

  // Destroy current renderer
  destroyRenderer();

  // Set new mode
  m_mode = mode;

  // Create new renderer
  createRenderer();
  
  DEBUG_LOG("Mode changed successfully to %s", 
           (m_mode == RENDER_MODE_DIRECT2D ? "Direct2D" : "GDI"));

  return true;
}

/**
 * Creates the appropriate renderer (GDI or Direct2D) based on the current mode
 * 
 * If Direct2D creation fails, it will fall back to GDI mode automatically
 */
void RenderManager::createRenderer()
{
  DEBUG_LOG("createRenderer: mode=%s, window=%p", 
           (m_mode == RENDER_MODE_DIRECT2D ? "Direct2D" : "GDI"), m_window);
  
  try {
    if (m_mode == RENDER_MODE_DIRECT2D) {
      // Make sure we have a valid window
      if (m_window == NULL || !IsWindow(m_window)) {
        DEBUG_LOG("Invalid window handle, using desktop window as fallback");
        // Use desktop window as fallback if window handle is invalid
        m_window = GetDesktopWindow();
      }
      
      // Only try to create Direct2D section if we have a valid window now
      if (m_window != NULL && IsWindow(m_window)) {
        DEBUG_LOG("Creating Direct2DSection");
        m_direct2DSection = new Direct2DSection(&m_pixelFormat, &m_dimension, m_window);
        DEBUG_LOG("Direct2DSection created successfully");
      } else {
        // Fallback to GDI if no valid window
        DEBUG_LOG("No valid window available, falling back to GDI");
        m_mode = RENDER_MODE_GDI;
        m_dibSection = new DibSection(&m_pixelFormat, &m_dimension, m_window);
        DEBUG_LOG("DibSection created successfully");
      }
    } else {
      DEBUG_LOG("Creating DibSection %p %dx%d", m_window, m_dimension.width, m_dimension.height);
      m_dibSection = new DibSection(&m_pixelFormat, &m_dimension, m_window);
      DEBUG_LOG("DibSection created successfully");
    }
  } catch (SystemException &e) {
    // If Direct2D creation failed, fall back to GDI
    if (m_mode == RENDER_MODE_DIRECT2D) {
      DEBUG_LOG("Exception creating Direct2DSection: %s, falling back to GDI", e.getMessage());
      m_mode = RENDER_MODE_GDI;
      m_direct2DSection = NULL;
      m_dibSection = new DibSection(&m_pixelFormat, &m_dimension, m_window);
      DEBUG_LOG("DibSection created successfully as fallback");
    } else {
      DEBUG_LOG("Exception creating DibSection: %s", e.getMessage());
      throw; // Re-throw for GDI failures
    }
  } catch (...) {
    // Catch any other exceptions and fallback to GDI
    if (m_mode == RENDER_MODE_DIRECT2D) {
      DEBUG_LOG("Unknown exception creating Direct2DSection, falling back to GDI");
      m_mode = RENDER_MODE_GDI;
      m_direct2DSection = NULL;
      try {
        m_dibSection = new DibSection(&m_pixelFormat, &m_dimension, m_window);
        DEBUG_LOG("DibSection created successfully as fallback");
      } catch (...) {
        // If even GDI fails, we have a serious problem
        DEBUG_LOG("Failed to create any renderer, throwing exception");
        throw SystemException(_T("Failed to create any renderer"));
      }
    } else {
      DEBUG_LOG("Unknown exception creating DibSection");
      throw; // Re-throw for GDI failures
    }
  }
}

/**
 * Destroys the current renderer and releases all resources
 */
void RenderManager::destroyRenderer()
{
  DEBUG_LOG("destroyRenderer called");
  
  if (m_direct2DSection) {
    DEBUG_LOG("Destroying Direct2DSection");
    delete m_direct2DSection;
    m_direct2DSection = NULL;
  }

  if (m_dibSection) {
    DEBUG_LOG("Destroying DibSection");
    delete m_dibSection;
    m_dibSection = NULL;
  }
  
  DEBUG_LOG("destroyRenderer complete");
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
  DEBUG_LOG("Checking if Direct2D is available");
  
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
    DEBUG_LOG("Direct2D is available");
    return true;
  }
  
  DEBUG_LOG("Direct2D is NOT available, HRESULT: 0x%lx", hr);
  return false;
}

void RenderManager::resize(const Rect* newSize) {
  if (m_mode == RENDER_MODE_DIRECT2D) {
    m_direct2DSection->resize(newSize);
  }
}