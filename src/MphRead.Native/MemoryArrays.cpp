#include "MemoryArrays.hpp"

#include <algorithm>
#include <stdexcept>

namespace fruityprime::memory {

ByteArrayView::ByteArrayView(Buffer& buffer, std::size_t offset,
                             std::size_t count)
    : buffer_(&buffer), offset_(offset), count_(count) {
    if (!buffer.contains(offset, count)) {
        throw std::out_of_range("memory array is outside the buffer");
    }
}

std::uint8_t ByteArrayView::get(std::size_t index) const {
    if (index >= count_) {
        throw std::out_of_range("memory array index is outside the view");
    }
    return buffer_->read_u8(offset_ + index);
}

void ByteArrayView::set(std::size_t index, std::uint8_t value) {
    if (index >= count_) {
        throw std::out_of_range("memory array index is outside the view");
    }
    buffer_->write_u8(offset_ + index, value);
}

void ByteArrayView::fill(std::uint8_t value) noexcept {
    const auto bytes = buffer_->writable_bytes().subspan(offset_, count_);
    std::fill(bytes.begin(), bytes.end(), value);
}

std::vector<std::uint8_t> ByteArrayView::copy() const {
    const auto bytes = buffer_->bytes().subspan(offset_, count_);
    return {bytes.begin(), bytes.end()};
}

} // namespace fruityprime::memory
