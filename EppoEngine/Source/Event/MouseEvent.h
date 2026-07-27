#pragma once

#include "Core/MouseCodes.h"
#include "Event/Event.h"

namespace Eppo
{
    class MouseMovedEvent : public Event
    {
    public:
        MouseMovedEvent(const float x, const float y)
            : m_MouseX(x), m_MouseY(y)
        {}

        [[nodiscard]] auto GetX() const -> float { return m_MouseX; }
        [[nodiscard]] auto GetY() const -> float { return m_MouseY; }

        [[nodiscard]] auto ToString() const -> std::string override
        {
            std::stringstream ss;
            ss << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;

            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)

    private:
        float m_MouseX;
        float m_MouseY;
    };

    class MouseScrolledEvent : public Event
    {
    public:
        MouseScrolledEvent(const float xOffset, const float yOffset)
            : m_xOffset(xOffset), m_yOffset(yOffset)
        {}

        [[nodiscard]] auto GetXOffset() const -> float { return m_xOffset; }
        [[nodiscard]] auto GetYOffset() const -> float { return m_yOffset; }

        [[nodiscard]] auto ToString() const -> std::string override
        {
            std::stringstream ss;
            ss << "MouseScrolledEvent: " << GetXOffset() << ", " << GetYOffset();

            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)

    private:
        float m_xOffset;
        float m_yOffset;
    };

    class MouseButtonEvent : public Event
    {
    public:
        [[nodiscard]] auto GetMouseButton() const { return m_Button; }

        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)

    protected:
        MouseButtonEvent(const MouseCode button)
            : m_Button(button)
        {}

        MouseCode m_Button;
    };

    class MouseButtonPressedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonPressedEvent(const MouseCode button)
            : MouseButtonEvent(button)
        {}

        [[nodiscard]] auto ToString() const -> std::string override
        {
            std::stringstream ss;
            ss << "MouseButtonPressedEvent: " << m_Button;

            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseButtonPressed)
    };

    class MouseButtonReleasedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonReleasedEvent(const MouseCode button)
            : MouseButtonEvent(button)
        {}

        [[nodiscard]] auto ToString() const -> std::string override
        {
            std::stringstream ss;
            ss << "MouseButtonReleasedEvent: " << m_Button;

            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseButtonReleased)
    };
}