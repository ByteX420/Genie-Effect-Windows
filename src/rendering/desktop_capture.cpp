#include "pch.hpp"

#include "rendering/desktop_capture.hpp"

#include <algorithm>
#include <iostream>

#include "common/debug_log.hpp"

namespace genie::rendering {
namespace {

int RectWidth(const RECT& rect) { return static_cast<int>(rect.right - rect.left); }

int RectHeight(const RECT& rect) { return static_cast<int>(rect.bottom - rect.top); }

bool ContainsRect(const RECT& outer, const RECT& inner) {
  return inner.left >= outer.left && inner.top >= outer.top && inner.right <= outer.right &&
         inner.bottom <= outer.bottom;
}

RECT ClampRectToOutput(const RECT& rect, const RECT& output_rect) {
  return RECT{
      .left = std::max(rect.left, output_rect.left),
      .top = std::max(rect.top, output_rect.top),
      .right = std::min(rect.right, output_rect.right),
      .bottom = std::min(rect.bottom, output_rect.bottom),
  };
}

bool IsDeviceLostError(HRESULT hr) {
  return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ||
         hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

int GetWindowCornerRadius(HWND window) {
  if (window == nullptr || !IsWindow(window)) {
    return 0;
  }
  if (IsZoomed(window)) {
    return 0;
  }

  // 33 is DWMWA_WINDOW_CORNER_PREFERENCE
  DWORD corner_preference = 0;
  HRESULT hr = DwmGetWindowAttribute(window, static_cast<DWMWINDOWATTRIBUTE>(33),
                                     &corner_preference, sizeof(corner_preference));
  if (FAILED(hr)) {
    return 0;  // Not Windows 11
  }

  int base_radius = 12;
  if (corner_preference == 1) {  // DWMWCP_DONOTROUND
    return 0;
  } else if (corner_preference == 3) {  // DWMWCP_ROUNDSMALL
    base_radius = 8;
  }

  UINT dpi = GetDpiForWindow(window);
  if (dpi == 0) {
    dpi = 96;
  }
  return MulDiv(base_radius, dpi, 96);
}

void ApplyRoundedCornerMask(std::vector<std::uint8_t>* pixels, int width, int height, int radius,
                            const RECT& window_rect, const RECT& extended_bounds) {
  if (pixels == nullptr || width <= 0 || height <= 0) {
    return;
  }

  // 1. Initialize all alpha channel values to 0xff (fully opaque)
  for (size_t i = 3; i < pixels->size(); i += 4) {
    (*pixels)[i] = 0xff;
  }

  // 2. Determine visible bounds in image coordinates
  int visible_left = std::max(0, static_cast<int>(extended_bounds.left - window_rect.left));
  int visible_top = std::max(0, static_cast<int>(extended_bounds.top - window_rect.top));
  int visible_right = std::min(width, static_cast<int>(extended_bounds.right - window_rect.left));
  int visible_bottom = std::min(height, static_cast<int>(extended_bounds.bottom - window_rect.top));

  // If bounds are invalid, fall back to entire image
  if (visible_right <= visible_left || visible_bottom <= visible_top) {
    visible_left = 0;
    visible_top = 0;
    visible_right = width;
    visible_bottom = height;
  }

  auto clear_pixel = [pixels, width](int px, int py) {
    const size_t index =
        (static_cast<size_t>(py) * static_cast<size_t>(width) + static_cast<size_t>(px)) * 4 + 3;
    (*pixels)[index] = 0;
  };

  // 3. Clear everything outside the visible area (the DWM shadow border / resize margins)
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (x < visible_left || x >= visible_right || y < visible_top || y >= visible_bottom) {
        clear_pixel(x, y);
      }
    }
  }

  if (radius <= 0) {
    return;
  }

  // Limit radius to visible size / 2
  int visible_width = visible_right - visible_left;
  int visible_height = visible_bottom - visible_top;
  radius = std::min(radius, visible_width / 2);
  radius = std::min(radius, visible_height / 2);

  // 4. Clear alpha outside corners of the visible rect with anti-aliasing
  for (int y = 0; y < radius; ++y) {
    for (int x = 0; x < radius; ++x) {
      const float cx = static_cast<float>(radius) - 0.5f;
      const float cy = cx;
      const float dx = static_cast<float>(x) - cx;
      const float dy = static_cast<float>(y) - cy;
      const float d = std::sqrt(dx * dx + dy * dy);

      float alpha_factor = 1.0f;
      if (d > static_cast<float>(radius) + 0.5f) {
        alpha_factor = 0.0f;
      } else if (d > static_cast<float>(radius) - 0.5f) {
        alpha_factor = (static_cast<float>(radius) + 0.5f - d);
      }

      auto apply_alpha = [pixels, width, alpha_factor](int px, int py) {
        const size_t index =
            (static_cast<size_t>(py) * static_cast<size_t>(width) + static_cast<size_t>(px)) * 4 +
            3;
        (*pixels)[index] =
            static_cast<std::uint8_t>(static_cast<float>((*pixels)[index]) * alpha_factor);
      };

      if (alpha_factor < 1.0f) {
        // Top-Left: relative to (visible_left, visible_top)
        apply_alpha(visible_left + x, visible_top + y);

        // Top-Right: relative to (visible_right - 1, visible_top)
        apply_alpha(visible_right - 1 - x, visible_top + y);

        // Bottom-Left: relative to (visible_left, visible_bottom - 1)
        apply_alpha(visible_left + x, visible_bottom - 1 - y);

        // Bottom-Right: relative to (visible_right - 1, visible_bottom - 1)
        apply_alpha(visible_right - 1 - x, visible_bottom - 1 - y);
      }
    }
  }
}

}  // namespace

DesktopCapture::DesktopCapture(D3dDevice* d3d_device) : d3d_device_(d3d_device) {}

bool DesktopCapture::CaptureRegion(const RECT& screen_rect, CapturedTexture* captured_texture) {
  if (captured_texture == nullptr || RectWidth(screen_rect) <= 0 || RectHeight(screen_rect) <= 0) {
    return false;
  }
  if (outputs_.empty() && !InitializeOutputs()) {
    return false;
  }

  OutputCapture* output = FindOutputForRect(screen_rect);
  if (output == nullptr) {
    std::wcerr << L"No DXGI output contains the target window.\n";
    return false;
  }

  if (output->frame_history.empty()) {
    TryAcquireLatestFrame(output, 120);
  }

  if (output->frame_history.empty()) {
    std::wcerr << L"No cached desktop frame is available for the minimize "
                  L"animation yet.\n";
    return false;
  }

  return CopyRegionFromFrame(output, screen_rect, captured_texture);
}

bool DesktopCapture::CaptureWindow(HWND window, const RECT& requested_screen_rect,
                                   CapturedTexture* captured_texture, RECT* captured_screen_rect) {
  if (window == nullptr || !IsWindow(window) || captured_texture == nullptr ||
      captured_screen_rect == nullptr) {
    return false;
  }

  RECT window_rect{};
  if (!GetWindowRect(window, &window_rect)) {
    return false;
  }

  RECT capture_rect{};
  if (!IntersectRect(&capture_rect, &window_rect, &requested_screen_rect) ||
      RectWidth(capture_rect) <= 0 || RectHeight(capture_rect) <= 0) {
    capture_rect = window_rect;
  }

  const int window_width = RectWidth(window_rect);
  const int window_height = RectHeight(window_rect);
  const int capture_width = RectWidth(capture_rect);
  const int capture_height = RectHeight(capture_rect);
  if (window_width <= 0 || window_height <= 0 || capture_width <= 0 || capture_height <= 0) {
    return false;
  }

  HDC screen_dc = GetDC(nullptr);
  if (screen_dc == nullptr) {
    return false;
  }
  HDC memory_dc = CreateCompatibleDC(screen_dc);
  if (memory_dc == nullptr) {
    ReleaseDC(nullptr, screen_dc);
    return false;
  }

  BITMAPINFO bitmap_info{};
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.bmiHeader.biWidth = window_width;
  bitmap_info.bmiHeader.biHeight = -window_height;
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;

  void* bitmap_bits = nullptr;
  HBITMAP bitmap =
      CreateDIBSection(screen_dc, &bitmap_info, DIB_RGB_COLORS, &bitmap_bits, nullptr, 0);
  ReleaseDC(nullptr, screen_dc);
  if (bitmap == nullptr || bitmap_bits == nullptr) {
    DeleteDC(memory_dc);
    return false;
  }

  HGDIOBJ old_bitmap = SelectObject(memory_dc, bitmap);
  if (old_bitmap == nullptr || old_bitmap == HGDI_ERROR) {
    DeleteObject(bitmap);
    DeleteDC(memory_dc);
    return false;
  }
  RECT paint_rect{0, 0, window_width, window_height};
  HBRUSH black_brush = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  FillRect(memory_dc, &paint_rect, black_brush);

  constexpr UINT kPrintWindowRenderFullContent = 0x00000002;
  BOOL printed = PrintWindow(window, memory_dc, kPrintWindowRenderFullContent);
  if (printed == FALSE) {
    printed = PrintWindow(window, memory_dc, 0);
  }
  GdiFlush();

  std::vector<std::uint8_t> pixels(static_cast<size_t>(capture_width) *
                                   static_cast<size_t>(capture_height) * 4);
  if (printed != FALSE) {
    const auto* source_pixels = static_cast<const std::uint8_t*>(bitmap_bits);
    const int source_x = capture_rect.left - window_rect.left;
    const int source_y = capture_rect.top - window_rect.top;
    const size_t source_stride = static_cast<size_t>(window_width) * 4;
    const size_t dest_stride = static_cast<size_t>(capture_width) * 4;
    for (int row = 0; row < capture_height; ++row) {
      const auto* source_row = source_pixels + static_cast<size_t>(source_y + row) * source_stride +
                               static_cast<size_t>(source_x) * 4;
      auto* dest_row = pixels.data() + static_cast<size_t>(row) * dest_stride;
      std::memcpy(dest_row, source_row, dest_stride);
    }
    RECT extended_bounds{};
    HRESULT hr_ext = DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &extended_bounds,
                                           sizeof(extended_bounds));
    if (FAILED(hr_ext)) {
      extended_bounds = window_rect;
    }
    const int radius = GetWindowCornerRadius(window);
    ApplyRoundedCornerMask(&pixels, capture_width, capture_height, radius, window_rect,
                           extended_bounds);
  }

  SelectObject(memory_dc, old_bitmap);
  DeleteObject(bitmap);
  DeleteDC(memory_dc);

  if (printed == FALSE) {
    LogTrace(L"DesktopCapture", L"CaptureWindow PrintWindow failed hwnd=0x" +
                                    std::to_wstring(reinterpret_cast<std::uintptr_t>(window)) +
                                    L" error=" + std::to_wstring(GetLastError()));
    return false;
  }

  D3D11_TEXTURE2D_DESC texture_desc{};
  texture_desc.Width = static_cast<UINT>(capture_width);
  texture_desc.Height = static_cast<UINT>(capture_height);
  texture_desc.MipLevels = 1;
  texture_desc.ArraySize = 1;
  texture_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  D3D11_SUBRESOURCE_DATA initial_data{};
  initial_data.pSysMem = pixels.data();
  initial_data.SysMemPitch = static_cast<UINT>(capture_width * 4);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  HRESULT hr = d3d_device_->device()->CreateTexture2D(&texture_desc, &initial_data, &texture);
  if (FAILED(hr)) {
    if (IsDeviceLostError(hr)) {
      MarkDeviceLost(L"CreateTexture2D PrintWindow capture", hr);
      return false;
    }
    LogTrace(L"DesktopCapture", L"CaptureWindow CreateTexture2D failed hr=0x" +
                                    std::to_wstring(static_cast<unsigned long>(hr)));
    return false;
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
  srv_desc.Format = texture_desc.Format;
  srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture2D.MipLevels = 1;

  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
  hr = d3d_device_->device()->CreateShaderResourceView(texture.Get(), &srv_desc,
                                                       &shader_resource_view);
  if (FAILED(hr)) {
    if (IsDeviceLostError(hr)) {
      MarkDeviceLost(L"CreateShaderResourceView PrintWindow capture", hr);
      return false;
    }
    LogTrace(L"DesktopCapture", L"CaptureWindow CreateShaderResourceView failed hr=0x" +
                                    std::to_wstring(static_cast<unsigned long>(hr)));
    return false;
  }

  captured_texture->texture = texture;
  captured_texture->shader_resource_view = shader_resource_view;
  captured_texture->size = genie::animation::SizeF{
      .width = static_cast<float>(capture_width),
      .height = static_cast<float>(capture_height),
  };
  *captured_screen_rect = capture_rect;
  LogTrace(L"DesktopCapture",
           L"CaptureWindow succeeded hwnd=0x" +
               std::to_wstring(reinterpret_cast<std::uintptr_t>(window)) + L" size=" +
               std::to_wstring(capture_width) + L"x" + std::to_wstring(capture_height) +
               L" capture_rect=(" + std::to_wstring(capture_rect.left) + L"," +
               std::to_wstring(capture_rect.top) + L"," + std::to_wstring(capture_rect.right) +
               L"," + std::to_wstring(capture_rect.bottom) + L") window_rect=(" +
               std::to_wstring(window_rect.left) + L"," + std::to_wstring(window_rect.top) + L"," +
               std::to_wstring(window_rect.right) + L"," + std::to_wstring(window_rect.bottom) +
               L")");
  return true;
}

bool DesktopCapture::RefreshCapturedTexture(const RECT& screen_rect,
                                            CapturedTexture* captured_texture) {
  if (captured_texture == nullptr || captured_texture->texture == nullptr ||
      RectWidth(screen_rect) <= 0 || RectHeight(screen_rect) <= 0) {
    return false;
  }
  if (outputs_.empty() && !InitializeOutputs()) {
    return false;
  }

  OutputCapture* output = FindOutputForRect(screen_rect);
  if (output == nullptr) {
    return false;
  }

  TryAcquireLatestFrame(output, 0);
  if (output->frame_history.empty()) {
    return false;
  }

  return CopyRegionIntoTexture(output, screen_rect, captured_texture);
}

void DesktopCapture::RefreshFrames(UINT timeout_ms) {
  if (outputs_.empty() && !InitializeOutputs()) {
    return;
  }

  for (auto& output : outputs_) {
    TryAcquireLatestFrame(&output, timeout_ms);
  }
}

bool DesktopCapture::TryAcquireLatestFrame(OutputCapture* output, UINT timeout_ms) {
  if (output == nullptr || output->duplication == nullptr) {
    return false;
  }

  DXGI_OUTDUPL_FRAME_INFO frame_info{};
  Microsoft::WRL::ComPtr<IDXGIResource> desktop_resource;
  HRESULT hr = output->duplication->AcquireNextFrame(timeout_ms, &frame_info, &desktop_resource);
  if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
    return !output->frame_history.empty();
  }
  if (hr == DXGI_ERROR_ACCESS_LOST) {
    ResetOutputs();
    return false;
  }
  if (IsDeviceLostError(hr)) {
    MarkDeviceLost(L"AcquireNextFrame", hr);
    return false;
  }
  if (FAILED(hr)) {
    std::wcerr << L"IDXGIOutputDuplication::AcquireNextFrame failed: 0x" << std::hex << hr << L"\n";
    return !output->frame_history.empty();
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> desktop_texture;
  hr = desktop_resource.As(&desktop_texture);
  if (FAILED(hr)) {
    output->duplication->ReleaseFrame();
    std::wcerr << L"Desktop frame is not a D3D11 texture: 0x" << std::hex << hr << L"\n";
    return false;
  }

  D3D11_TEXTURE2D_DESC desktop_desc{};
  desktop_texture->GetDesc(&desktop_desc);
  output->latest_frame_format = desktop_desc.Format;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> target_texture;
  if (output->frame_history.size() < kHistorySize) {
    D3D11_TEXTURE2D_DESC cached_desc = desktop_desc;
    cached_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    cached_desc.CPUAccessFlags = 0;
    cached_desc.MiscFlags = 0;
    cached_desc.Usage = D3D11_USAGE_DEFAULT;

    hr = d3d_device_->device()->CreateTexture2D(&cached_desc, nullptr, &target_texture);
    if (FAILED(hr)) {
      output->duplication->ReleaseFrame();
      if (IsDeviceLostError(hr)) {
        MarkDeviceLost(L"CreateTexture2D cached desktop frame", hr);
        return false;
      }
      std::wcerr << L"CreateTexture2D for cached desktop frame failed: 0x" << std::hex << hr
                 << L"\n";
      return false;
    }
    output->frame_history.push_back(target_texture);
    output->current_frame_index = output->frame_history.size() - 1;
  } else {
    output->current_frame_index = (output->current_frame_index + 1) % kHistorySize;
    target_texture = output->frame_history[output->current_frame_index];
  }

  d3d_device_->context()->CopyResource(target_texture.Get(), desktop_texture.Get());
  output->duplication->ReleaseFrame();
  return true;
}

bool DesktopCapture::CopyRegionFromFrame(OutputCapture* output, const RECT& screen_rect,
                                         CapturedTexture* captured_texture) {
  if (output == nullptr || output->frame_history.empty() || captured_texture == nullptr) {
    return false;
  }

  const RECT clipped_rect = ClampRectToOutput(screen_rect, output->desktop_coordinates);
  const int width = RectWidth(clipped_rect);
  const int height = RectHeight(clipped_rect);
  if (width <= 0 || height <= 0) {
    return false;
  }

  size_t oldest_index = 0;
  if (output->frame_history.size() == kHistorySize) {
    oldest_index = (output->current_frame_index + 1) % kHistorySize;
  }
  ID3D11Texture2D* source_frame = output->frame_history[oldest_index].Get();

  D3D11_TEXTURE2D_DESC texture_desc{};
  texture_desc.Width = static_cast<UINT>(width);
  texture_desc.Height = static_cast<UINT>(height);
  texture_desc.MipLevels = 1;
  texture_desc.ArraySize = 1;
  texture_desc.Format = output->latest_frame_format;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> copied_texture;
  HRESULT hr = d3d_device_->device()->CreateTexture2D(&texture_desc, nullptr, &copied_texture);
  if (FAILED(hr)) {
    if (IsDeviceLostError(hr)) {
      MarkDeviceLost(L"CreateTexture2D captured window", hr);
      return false;
    }
    std::wcerr << L"CreateTexture2D for captured window failed: 0x" << std::hex << hr << std::dec
               << L" size=" << width << L"x" << height << L" format=" << texture_desc.Format
               << L"\n";
    return false;
  }

  D3D11_BOX source_box{};
  source_box.left = static_cast<UINT>(clipped_rect.left - output->desktop_coordinates.left);
  source_box.top = static_cast<UINT>(clipped_rect.top - output->desktop_coordinates.top);
  source_box.front = 0;
  source_box.right = static_cast<UINT>(clipped_rect.right - output->desktop_coordinates.left);
  source_box.bottom = static_cast<UINT>(clipped_rect.bottom - output->desktop_coordinates.top);
  source_box.back = 1;

  d3d_device_->context()->CopySubresourceRegion(copied_texture.Get(), 0, 0, 0, 0, source_frame, 0,
                                                &source_box);

  D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
  srv_desc.Format = texture_desc.Format;
  srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture2D.MipLevels = 1;

  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
  hr = d3d_device_->device()->CreateShaderResourceView(copied_texture.Get(), &srv_desc,
                                                       &shader_resource_view);
  if (FAILED(hr)) {
    if (IsDeviceLostError(hr)) {
      MarkDeviceLost(L"CreateShaderResourceView captured window", hr);
      return false;
    }
    std::wcerr << L"CreateShaderResourceView for captured window failed: 0x" << std::hex << hr
               << L"\n";
    return false;
  }

  captured_texture->texture = copied_texture;
  captured_texture->shader_resource_view = shader_resource_view;
  captured_texture->size = genie::animation::SizeF{
      .width = static_cast<float>(width),
      .height = static_cast<float>(height),
  };
  return true;
}

bool DesktopCapture::CopyRegionIntoTexture(OutputCapture* output, const RECT& screen_rect,
                                           CapturedTexture* captured_texture) {
  if (output == nullptr || output->frame_history.empty() || captured_texture == nullptr ||
      captured_texture->texture == nullptr) {
    return false;
  }

  const RECT clipped_rect = ClampRectToOutput(screen_rect, output->desktop_coordinates);
  const int width = RectWidth(clipped_rect);
  const int height = RectHeight(clipped_rect);
  if (width <= 0 || height <= 0) {
    return false;
  }

  D3D11_TEXTURE2D_DESC texture_desc{};
  captured_texture->texture->GetDesc(&texture_desc);
  if (texture_desc.Width != static_cast<UINT>(width) ||
      texture_desc.Height != static_cast<UINT>(height)) {
    return false;
  }

  ID3D11Texture2D* source_frame = output->frame_history[output->current_frame_index].Get();

  D3D11_BOX source_box{};
  source_box.left = static_cast<UINT>(clipped_rect.left - output->desktop_coordinates.left);
  source_box.top = static_cast<UINT>(clipped_rect.top - output->desktop_coordinates.top);
  source_box.front = 0;
  source_box.right = static_cast<UINT>(clipped_rect.right - output->desktop_coordinates.left);
  source_box.bottom = static_cast<UINT>(clipped_rect.bottom - output->desktop_coordinates.top);
  source_box.back = 1;

  d3d_device_->context()->CopySubresourceRegion(captured_texture->texture.Get(), 0, 0, 0, 0,
                                                source_frame, 0, &source_box);
  return true;
}

bool DesktopCapture::InitializeOutputs() {
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  HRESULT hr = d3d_device_->dxgi_device()->GetAdapter(&adapter);
  if (FAILED(hr)) {
    return false;
  }

  for (UINT index = 0;; ++index) {
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    hr = adapter->EnumOutputs(index, &output);
    if (hr == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (FAILED(hr)) {
      continue;
    }

    DXGI_OUTPUT_DESC output_desc{};
    output->GetDesc(&output_desc);

    Microsoft::WRL::ComPtr<IDXGIOutput1> output1;
    hr = output.As(&output1);
    if (FAILED(hr)) {
      continue;
    }

    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication;
    hr = output1->DuplicateOutput(d3d_device_->device(), &duplication);
    if (FAILED(hr)) {
      std::wcerr << L"DuplicateOutput failed for monitor " << index << L": 0x" << std::hex << hr
                 << L"\n";
      continue;
    }

    outputs_.push_back(OutputCapture{
        .desktop_coordinates = output_desc.DesktopCoordinates,
        .duplication = duplication,
    });
  }

  return !outputs_.empty();
}

DesktopCapture::OutputCapture* DesktopCapture::FindOutputForRect(const RECT& screen_rect) {
  for (auto& output : outputs_) {
    if (ContainsRect(output.desktop_coordinates, screen_rect)) {
      return &output;
    }
  }

  const LONG center_x = screen_rect.left + RectWidth(screen_rect) / 2;
  const LONG center_y = screen_rect.top + RectHeight(screen_rect) / 2;
  for (auto& output : outputs_) {
    const RECT& rect = output.desktop_coordinates;
    if (center_x >= rect.left && center_x < rect.right && center_y >= rect.top &&
        center_y < rect.bottom) {
      return &output;
    }
  }

  return nullptr;
}

void DesktopCapture::ResetOutputs() { outputs_.clear(); }

void DesktopCapture::MarkDeviceLost(const wchar_t* context, HRESULT hr) {
  device_lost_ = true;
  ResetOutputs();
  HRESULT reason = S_OK;
  if (d3d_device_ != nullptr && d3d_device_->device() != nullptr) {
    reason = d3d_device_->device()->GetDeviceRemovedReason();
  }
  std::wcerr << L"D3D device lost during " << context << L": hr=0x" << std::hex << hr
             << L", reason=0x" << reason << std::dec << L". Reinitializing renderer.\n";
}

}  // namespace genie::rendering
