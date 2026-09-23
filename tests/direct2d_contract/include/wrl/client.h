#pragma once

#include <memory>
#include <utility>

namespace Microsoft::WRL {

template <typename T>
class ComPtr final {
public:
    constexpr ComPtr() noexcept = default;
    constexpr ComPtr(std::nullptr_t) noexcept {
    }

    explicit ComPtr(T* value) noexcept
        : value_(value) {
        internalAddRef();
    }

    ComPtr(const ComPtr& other) noexcept
        : value_(other.value_) {
        internalAddRef();
    }

    ComPtr(ComPtr&& other) noexcept
        : value_(std::exchange(other.value_, nullptr)) {
    }

    ComPtr& operator=(const ComPtr& other) noexcept {
        if (this != std::addressof(other)) {
            ComPtr copy(other);
            swap(copy);
        }
        return *this;
    }

    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != std::addressof(other)) {
            internalRelease();
            value_ = std::exchange(other.value_, nullptr);
        }
        return *this;
    }

    ~ComPtr() {
        internalRelease();
    }

    [[nodiscard]] T* Get() const noexcept { return value_; }
    [[nodiscard]] T* operator->() const noexcept { return value_; }
    [[nodiscard]] explicit operator bool() const noexcept { return value_ != nullptr; }

    // Matches the WRL usage employed by the production backend. The out
    // parameter takes ownership of the returned reference.
    [[nodiscard]] T** operator&() noexcept {
        Reset();
        return &value_;
    }

    [[nodiscard]] T** GetAddressOf() noexcept { return &value_; }
    [[nodiscard]] T* const* GetAddressOf() const noexcept { return &value_; }

    void Reset() noexcept {
        internalRelease();
    }

    [[nodiscard]] T* Detach() noexcept {
        return std::exchange(value_, nullptr);
    }

    void Attach(T* value) noexcept {
        internalRelease();
        value_ = value;
    }

    void swap(ComPtr& other) noexcept {
        std::swap(value_, other.value_);
    }

private:
    void internalAddRef() noexcept {
        if (value_ != nullptr) {
            value_->AddRef();
        }
    }

    void internalRelease() noexcept {
        auto* old = std::exchange(value_, nullptr);
        if (old != nullptr) {
            old->Release();
        }
    }

    T* value_ = nullptr;
};

} // namespace Microsoft::WRL
