// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include <memory>
#include <utility>

#include "ImageDecoder.hpp"
#include "system/Path.hpp"
#include "ui/canvas/custom/UncompressedImage.hpp"
#include "util/ScopeExit.hxx"
#include "LogFile.hpp"

#import <CoreGraphics/CoreGraphics.h>

static UncompressedImage
CGImageToUncompressedImage(CGImageRef image) noexcept
{
  if (image == nullptr)
    return {};

  size_t width = CGImageGetWidth(image);
  size_t height = CGImageGetHeight(image);

  if ((0 == width) || (0 == height))
    return {};

  size_t bits_per_pixel = CGImageGetBitsPerPixel(image);
  size_t bits_per_component = CGImageGetBitsPerComponent(image);
  CGColorSpaceRef colorspace = CGImageGetColorSpace(image);

  size_t row_size;
  UncompressedImage::Format format;
  CGColorSpaceRef bitmap_colorspace;
  uint32_t bitmap_info;

  if ((8 == bits_per_pixel) &&
      (8 == bits_per_component) &&
      (CGColorSpaceGetModel(colorspace) == kCGColorSpaceModelMonochrome)) {
		// LogFormat("===>appleUncompressedImage 8");
    row_size = width;
    format = UncompressedImage::Format::GRAY;
    static const CGColorSpaceRef grey_colorspace = CGColorSpaceCreateDeviceGray();
    bitmap_colorspace = grey_colorspace;
    bitmap_info = 0;
  } else {
	// LogFormat("===>appleUncompressedImage NOT 8");
    static const CGColorSpaceRef rgb_colorspace = CGColorSpaceCreateDeviceRGB();
    bitmap_colorspace = rgb_colorspace;
    if ((24 == bits_per_pixel) && (8 == bits_per_component)) {
      row_size = width * 3;
      format = UncompressedImage::Format::RGB;
      bitmap_info = kCGBitmapByteOrder32Big;
    } else {
      row_size = width * 4;
      format = UncompressedImage::Format::RGBA;
      bitmap_info = static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) |
        static_cast<uint32_t>(kCGBitmapByteOrder32Big);
    }
  }

  std::unique_ptr<uint8_t[]> uncompressed(new uint8_t[height * row_size]);

  CGContextRef bitmap = CGBitmapContextCreate(uncompressed.get(), width, height,
                                              8, row_size, bitmap_colorspace,
                                              bitmap_info);
  if (nullptr == bitmap) {
    return {};
  }

  AtScopeExit(bitmap) { CFRelease(bitmap); };

  CGContextDrawImage(bitmap, CGRectMake(0, 0, width, height), image);

// LogFormat("===>CGBitmapContextCreate");
  return UncompressedImage(format, row_size, width, height,
                           std::move(uncompressed));
}

UncompressedImage
LoadJPEGFile(Path path) noexcept
{
	// LogFormat("===>LoadJPEGFile");
  CGDataProviderRef data_provider = CGDataProviderCreateWithFilename(path.c_str());
  if (nullptr == data_provider)
    return {};

  CGImageRef image =  CGImageCreateWithJPEGDataProvider(
      data_provider, nullptr, false, kCGRenderingIntentDefault);

  UncompressedImage result = CGImageToUncompressedImage(image);

  if (nullptr != image)
    CFRelease(image);
  CFRelease(data_provider);

  return result;
}

UncompressedImage
LoadPNG(std::span<const std::byte> raw)
{
// LogFormat("===>LoadPNG raw");
  assert(raw.data() != nullptr);

  CGDataProviderRef data_provider = CGDataProviderCreateWithData(
      nullptr, raw.data(), raw.size(), nullptr);
  if (nullptr == data_provider)
    return {};

  CGImageRef image = CGImageCreateWithPNGDataProvider(
      data_provider, nullptr, false, kCGRenderingIntentDefault);

// size_t width = CGImageGetWidth(image);
// size_t height = CGImageGetHeight(image);
// size_t bitsPerComponent = CGImageGetBitsPerComponent(image);
// size_t bytesPerRow = CGImageGetBytesPerRow(image);
// CGColorSpaceRef colorSpace = CGImageGetColorSpace(image);
// CGBitmapInfo bitmapInfo = CGImageGetBitmapInfo(image);

// LogFormat("Image width: %zu", width);
// 		LogFormat("Image height: %zu", height);
// 		LogFormat("Bits per component: %zu", bitsPerComponent);
// 		LogFormat("Bytes per row: %zu", bytesPerRow);
// 		LogFormat("Color space: %p", colorSpace);
// 		LogFormat("Bitmap info: 0x%08X", bitmapInfo);

		
// if (colorSpace != nullptr) {
// 	CFStringRef name = CGColorSpaceCopyName(colorSpace);
// 	if (name) {
// 		char buffer[128];
// 		CFStringGetCString(name, buffer, sizeof(buffer), kCFStringEncodingUTF8);
// 		// LogFormat("Color space name: %s", buffer);
// 		CFRelease(name);
// 	} else {
// 		// LogFormat("Color space has no name (custom or device-dependent)");
// 	}
// } else {
// 	// LogFormat("No color space set");
// }


  UncompressedImage result = CGImageToUncompressedImage(image);

  if (nullptr != image)
    CFRelease(image);
  CFRelease(data_provider);

  return result;
}

UncompressedImage
LoadPNG(Path path) noexcept
{
	// LogFormat("===>LoadPNG path %s", path.c_str());
  CGDataProviderRef data_provider = CGDataProviderCreateWithFilename(path.c_str());
  if (nullptr == data_provider)
    return {};

  CGImageRef image =  CGImageCreateWithPNGDataProvider(
      data_provider, nullptr, false, kCGRenderingIntentDefault);

  UncompressedImage result = CGImageToUncompressedImage(image);

  if (nullptr != image)
    CFRelease(image);
  CFRelease(data_provider);
  return result;
}
