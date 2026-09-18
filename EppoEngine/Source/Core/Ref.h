#pragma once

#include <atomic>
#include <concepts>
#include <memory>

namespace Eppo
{
    auto AddLiveRef(void* instance) -> void;
    auto RemoveLiveRef(void* instance) -> void;
    auto IsLive(void* instance) -> bool;

    class RefCtr
    {
    public:
        virtual ~RefCtr() = default;

        auto IncrementRefCount() const -> void { m_RefCount.fetch_add(1, std::memory_order_relaxed); }
        auto DecrementRefCount() const -> uint32_t { return m_RefCount.fetch_sub(1, std::memory_order_acq_rel) - 1; }
        auto GetRefCount() const -> uint32_t { return m_RefCount.load(std::memory_order_relaxed); }

    private:
        // mutable + const methods allows for example: const auto& x = refcountedobject
        mutable std::atomic<uint32_t> m_RefCount = 0;
    };

    template<typename T>
    class Ref
    {
    public:
        Ref() = default;
        ~Ref() { DecrementRef(); }

        // Nullptr constructor
        Ref(std::nullptr_t) {}

        // Raw ptr constructor
        Ref(T* instance)
        {
            static_assert(std::derived_from<T, RefCtr>, "Class is not being reference counted!");
            m_Instance = instance;
            IncrementRef();
        }

        // Copy constructor
        Ref(const Ref<T>& other)
        {
            m_Instance = other.m_Instance;
            IncrementRef();
        }

        // Move constructor
        Ref(Ref<T>&& other) noexcept
        {
            m_Instance = other.m_Instance;
            other.m_Instance = nullptr;
        }

        // Copy constructor from/to derived
        template<typename T2>
            requires(std::derived_from<T2, T> || std::derived_from<T, T2>)
        Ref(const Ref<T2>& other)
        {
            m_Instance = static_cast<T*>(other.m_Instance);
            IncrementRef();
        }

        // Move constructor from/to derived
        template<typename T2>
            requires(std::derived_from<T2, T> || std::derived_from<T, T2>)
        Ref(Ref<T2>&& other)
        {
            m_Instance = static_cast<T*>(other.m_Instance);
            other.m_Instance = nullptr;
        }

        friend auto operator==(const Ref& lhs, const Ref& rhs) -> bool { return lhs.m_Instance == rhs.m_Instance; }
        friend auto operator==(const Ref& lhs, std::nullptr_t) -> bool { return lhs.m_Instance == nullptr; }
        friend auto operator==(std::nullptr_t, const Ref& rhs) -> bool { return nullptr == rhs.m_Instance; }
        friend auto operator!=(const Ref& lhs, const Ref& rhs) -> bool { return lhs.m_Instance != rhs.m_Instance; }
        friend auto operator!=(const Ref& lhs, std::nullptr_t) -> bool { return lhs.m_Instance != nullptr; }
        friend auto operator!=(std::nullptr_t, const Ref& rhs) -> bool { return nullptr != rhs.m_Instance; }

        // Null assignment
        auto operator=(std::nullptr_t) -> Ref&
        {
            DecrementRef();
            m_Instance = nullptr;

            return *this;
        }

        // Copy assignment
        auto operator=(const Ref<T>& other) -> Ref&
        {
            if (this == &other)
                return *this;

            other.IncrementRef();
            DecrementRef();
            m_Instance = other.m_Instance;

            return *this;
        }

        // Move assignment
        auto operator=(Ref<T>&& other) noexcept -> Ref&
        {
            if (this != &other)
            {
                DecrementRef();
                m_Instance = other.m_Instance;
                other.m_Instance = nullptr;
            }

            return *this;
        }

        // Copy assignment from/to derived
        template<typename T2>
            requires(std::derived_from<T2, T> || std::derived_from<T, T2>)
        auto operator=(const Ref<T2>& other) -> Ref&
        {
            other.IncrementRef();
            DecrementRef();
            m_Instance = static_cast<T*>(other.m_Instance);

            return *this;
        }

        // Move assignment from/to derived
        template<typename T2>
            requires(std::derived_from<T2, T> || std::derived_from<T, T2>)
        auto operator=(Ref<T2>&& other) noexcept -> Ref&
        {
            if (this != &other)
            {
                DecrementRef();
                m_Instance = static_cast<T*>(other.m_Instance);
                other.m_Instance = nullptr;
            }

            return *this;
        }

        explicit operator bool() const { return m_Instance != nullptr; }

        auto operator->() -> T* { return m_Instance; }
        auto operator->() const -> const T* { return m_Instance; }
        auto operator*() -> T& { return *m_Instance; }
        auto operator*() const -> const T& { return *m_Instance; }
        auto Raw() -> T* { return m_Instance; }
        auto Raw() const -> T* { return m_Instance; }

        auto Reset(T* instance = nullptr) -> void
        {
            if (instance == m_Instance)
                return;

            DecrementRef();
            m_Instance = instance;
            IncrementRef();
        }

        template<typename T2>
            requires(std::derived_from<T2, T> || std::derived_from<T, T2>)
        auto As() const -> Ref<T2>
        {
            return Ref<T2>(*this);
        }

        template<typename... Args>
        static auto Create(Args&&... args) -> Ref<T>
        {
            return Ref<T>(new T(std::forward<Args>(args)...));
        }

    private:
        auto IncrementRef() const -> void
        {
            if (m_Instance)
            {
                m_Instance->IncrementRefCount();
                AddLiveRef(reinterpret_cast<void*>(m_Instance));
            }
        }

        auto DecrementRef() const -> void
        {
            if (m_Instance)
            {
                if (m_Instance->DecrementRefCount() == 0)
                {
                    delete m_Instance;
                    RemoveLiveRef(m_Instance);
                    m_Instance = nullptr;
                }
            }
        }

    private:
        template<typename T2>
        friend class Ref;
        mutable T* m_Instance = nullptr;
    };

    // Type-erased owning handle. The object pointer is kept for identity and raw access,
    // while counting runs through the RefCtr subobject.
    template<>
    class Ref<void>
    {
    public:
        Ref() = default;
        ~Ref() { DecrementRef(); }

        Ref(std::nullptr_t) {}

        Ref(const Ref& other)
        {
            m_Instance = other.m_Instance;
            m_Counter = other.m_Counter;
            IncrementRef();
        }

        Ref(Ref&& other) noexcept
        {
            m_Instance = other.m_Instance;
            m_Counter = other.m_Counter;
            other.m_Instance = nullptr;
            other.m_Counter = nullptr;
        }

        template<typename T2>
            requires(std::derived_from<T2, RefCtr>)
        Ref(const Ref<T2>& other)
        {
            m_Instance = other.m_Instance;
            m_Counter = other.m_Instance;
            IncrementRef();
        }

        friend auto operator==(const Ref& lhs, const Ref& rhs) -> bool { return lhs.m_Instance == rhs.m_Instance; }
        friend auto operator==(const Ref& lhs, std::nullptr_t) -> bool { return lhs.m_Instance == nullptr; }
        friend auto operator==(std::nullptr_t, const Ref& rhs) -> bool { return nullptr == rhs.m_Instance; }
        friend auto operator!=(const Ref& lhs, const Ref& rhs) -> bool { return lhs.m_Instance != rhs.m_Instance; }
        friend auto operator!=(const Ref& lhs, std::nullptr_t) -> bool { return lhs.m_Instance != nullptr; }
        friend auto operator!=(std::nullptr_t, const Ref& rhs) -> bool { return nullptr != rhs.m_Instance; }

        auto operator=(const Ref& other) -> Ref&
        {
            if (this == &other)
                return *this;

            other.IncrementRef();
            DecrementRef();
            m_Instance = other.m_Instance;
            m_Counter = other.m_Counter;

            return *this;
        }

        auto operator=(Ref&& other) noexcept -> Ref&
        {
            if (this != &other)
            {
                DecrementRef();
                m_Instance = other.m_Instance;
                m_Counter = other.m_Counter;
                other.m_Instance = nullptr;
                other.m_Counter = nullptr;
            }

            return *this;
        }

        template<typename T2>
            requires(std::derived_from<T2, RefCtr>)
        auto operator=(const Ref<T2>& other) -> Ref&
        {
            other.IncrementRef();
            DecrementRef();
            m_Instance = other.m_Instance;
            m_Counter = other.m_Instance;

            return *this;
        }

        explicit operator bool() const { return m_Instance != nullptr; }

        auto Raw() -> void* { return m_Instance; }
        auto Raw() const -> void* { return m_Instance; }

    private:
        auto IncrementRef() const -> void
        {
            if (m_Counter)
            {
                m_Counter->IncrementRefCount();
                AddLiveRef(m_Instance);
            }
        }

        auto DecrementRef() const -> void
        {
            if (m_Counter)
            {
                if (m_Counter->DecrementRefCount() == 0)
                {
                    delete m_Counter;
                    RemoveLiveRef(m_Instance);
                    m_Instance = nullptr;
                    m_Counter = nullptr;
                }
            }
        }

    private:
        mutable void* m_Instance = nullptr;
        mutable RefCtr* m_Counter = nullptr;
    };
}
