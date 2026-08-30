#pragma once

// clang-format off
#if defined(EP_PLATFORM_WINDOWS)
    #include <wrl/client.h>
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;
#else
    #include <dxc/WinAdapter.h>
    template<typename T>
    class ComPtr : public CComPtr<T>
    {
        using Base = CComPtr<T>;
    public:
        using Base::Base;
        using Base::operator=;

        ComPtr() noexcept = default;

        T* Get() const noexcept { return this->p; }

        T* const* GetAddressOf() const noexcept { return &this->p; }
        T** GetAddressOf() noexcept
        {
            assert(this->p == nullptr);
            return &this->p;
        }

        T** ReleaseAndGetAddressOf() noexcept
        {
            this->Release();
            return &this->p;
        }

        void Reset() noexcept { this->Release(); }
        explicit operator bool() const noexcept { return this->p != nullptr; }
    };
#endif
// clang-format on
