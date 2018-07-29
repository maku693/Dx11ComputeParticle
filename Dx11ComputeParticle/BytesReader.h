#pragma once

#include <cstdint>
#include <future>
#include <vector>

#include <winrt/Windows.Storage.Streams.h>

class BytesReader {
public:
  explicit BytesReader(const winrt::Windows::Storage::StorageFolder);

  std::future<std::vector<std::uint8_t>> Read(const winrt::hstring &);

private:
  const winrt::Windows::Storage::StorageFolder root_;
};
