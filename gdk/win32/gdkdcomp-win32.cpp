/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <../um/dcomp.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <glib.h>

extern "C" {
	void gdk_win32_set_shadow_effect (ID3D11Device        *d3d11_device,
                                      IDCompositionVisual *visual);
}

void gdk_win32_set_shadow_effect (ID3D11Device        *d3d11_device,
                                  IDCompositionVisual *visual)
{
  IDCompositionDevice *base_device;
  IDCompositionDevice3 *device3;
  HRESULT hr;
  IDXGIDevice *dxgi_device;
  IDCompositionShadowEffect *shadow_effect;
  IDCompositionEffect *effect;

  hr = d3d11_device->QueryInterface (__uuidof (IDXGIDevice), (void **) &dxgi_device);

  hr = DCompositionCreateDevice3 (dxgi_device, __uuidof (IDCompositionDevice), reinterpret_cast<void **>(&base_device));
  g_win32_clear_com (&dxgi_device);
  base_device->QueryInterface (__uuidof (IDCompositionDevice3), reinterpret_cast<void **>(&device3));
  g_message (SUCCEEDED (hr) ? "yay 2" : "boo 2");

  hr = device3->CreateShadowEffect (&shadow_effect);
  shadow_effect->SetAlpha (2.0f);
  shadow_effect->SetStandardDeviation (2.0f);
  hr = shadow_effect->QueryInterface (__uuidof (IDCompositionEffect), reinterpret_cast<void **>(&effect));
  visual->SetEffect (effect);
  g_message (SUCCEEDED (hr) ? "yay 4" : "boo 4");

}
