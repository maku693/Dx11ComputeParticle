#include "pch.h"

#include "BytesReader.h"

using namespace winrt;
using namespace Windows::ApplicationModel;
using namespace Windows::Storage;
using namespace Streams;

BytesReader::BytesReader(
    const StorageFolder root = Package::Current().InstalledLocation())
    : root_(root) {}

std::future<std::vector<std::uint8_t>> BytesReader::Read(const hstring &name) {
  const auto file = co_await root_.GetFileAsync(name);
  const auto buf = co_await FileIO::ReadBufferAsync(file);
  std::vector<std::uint8_t> data(buf.Length());
  DataReader::FromBuffer(buf).ReadBytes(data);
  co_return data;
}
