#pragma once

#include <sstream>

namespace Eppo
{
    enum class EventType : uint8_t
    {
        None = 0,
        WindowClose,
        WindowResize,
        WindowFocus,
        WindowLostFocus,
        WindowMoved,
        KeyPressed,
        KeyReleased,
        KeyTyped,
        MouseButtonPressed,
        MouseButtonReleased,
        MouseMoved,
        MouseScrolled
    };

    enum EventCategory
    {
        None = 0,
        EventCategoryApplication = 1 << 0,
        EventCategoryInput = 1 << 1,
        EventCategoryKeyboard = 1 << 2,
        EventCategoryMouse = 1 << 3,
        EventCategoryMouseButton = 1 << 4
    };

#define EVENT_CLASS_TYPE(type)                                                                                                             \
    static auto GetStaticType() -> EventType                                                                                               \
    {                                                                                                                                      \
        return EventType::type;                                                                                                            \
    }                                                                                                                                      \
    virtual auto GetEventType() const -> EventType override                                                                                \
    {                                                                                                                                      \
        return GetStaticType();                                                                                                            \
    }                                                                                                                                      \
    virtual auto GetName() const -> const char* override                                                                                   \
    {                                                                                                                                      \
        return #type;                                                                                                                      \
    }

#define EVENT_CLASS_CATEGORY(category)                                                                                                     \
    virtual auto GetCategoryFlags() const -> int override                                                                                  \
    {                                                                                                                                      \
        return category;                                                                                                                   \
    }

    struct Event
    {
        friend class EventDispatcher;

        virtual ~Event() = default;

        [[nodiscard]] virtual auto GetEventType() const -> EventType = 0;
        [[nodiscard]] virtual auto GetName() const -> const char* = 0;
        [[nodiscard]] virtual auto GetCategoryFlags() const -> int = 0;

        [[nodiscard]] virtual auto ToString() const -> std::string { return GetName(); }
        [[nodiscard]] auto IsInCategory(const EventCategory category) const -> bool { return GetCategoryFlags() & category; }

        bool Handled = false;
    };

    class EventDispatcher
    {
    public:
        EventDispatcher(Event& e)
            : m_Event(e)
        {}

        template<typename T, typename F>
        auto Dispatch(const F& fn) -> bool
        {
            if (m_Event.GetEventType() == T::GetStaticType())
            {
                m_Event.Handled |= fn(static_cast<T&>(m_Event));
                return true;
            }

            return false;
        }

    private:
        Event& m_Event;
    };
}