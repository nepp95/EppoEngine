#pragma once

#include "Core/Assert.h"

namespace Eppo
{
	class RefCounter
	{
	public:
		void IncreaseRefCount()
		{
			m_RefCount++;
		}

		void DecreaseRefCount()
		{
			m_RefCount--;
		}

		uint32_t GetRefCount() const
		{
			return m_RefCount;
		}

		template<typename T>
		friend class Ref;
		
	private:
		uint32_t m_RefCount = 0;
	};

	template<typename T>
	class Ref
	{
	public:
		// Default constructors
		Ref()
			: m_Object(nullptr)
		{}

		Ref(std::nullptr_t)
			: m_Object(nullptr)
		{}

		// Construct from raw pointer
		Ref(T* object)
		{
			static_assert(std::is_base_of_v<RefCounter, T>, "Class is not based on RefCounter!");

			m_Object = object;
			IncRef();
		}

		// Destructor
		~Ref()
		{
			DecRef();
		}
		
		// Copy constructor
		Ref(const Ref<T>& other)
		{
			m_Object = other.m_Object;
			IncRef();
		}

		template<typename T2>
		Ref(const Ref<T2>& other)
		{
			m_Object = (T*)other.m_Object;
			IncRef();
		}

		// Move constructor
		Ref(Ref<T>&& other)
		{
			m_Object = other.m_Object;
			other.m_Object = nullptr;
		}

		template<typename T2>
		Ref(Ref<T2>&& other)
		{
			m_Object = (T*)other.m_Object;
			other.m_Object = nullptr;
		}

		// Copy assignment operator
		Ref& operator=(const Ref<T>& other)
		{
			DecRef();
			m_Object = other.m_Object;
			IncRef();

			return *this;
		}

		template<typename T2>
		Ref& operator=(const Ref<T2>& other)
		{
			static_assert(std::is_base_of_v<T, T2> || std::is_base_of_v<T2, T>, "Class is not based on or used as base.");

			DecRef();
			m_Object = (T*)other.m_Object;
			IncRef();

			return *this;
		}

		// Move assignment operator
		Ref& operator=(Ref<T>&& other)
		{
			m_Object = other.m_Object;
			other.m_Object = nullptr;

			return *this;
		}

		template<typename T2>
		Ref& operator=(Ref<T2>&& other)
		{
			static_assert(std::is_base_of_v<T, T2> || std::is_base_of_v<T2, T>, "Class is not based on or used as base.");

			m_Object = (T*)other.m_Object;
			other.m_Object = nullptr;

			return *this;
		}

		// Raw pointer
		T* Raw() { return m_Object; }
		const T* Raw() const { return m_Object;	}

		// Pointer access
		T* operator->() { return m_Object; }
		const T* operator->() const { return m_Object; }

		// References
		T& operator*() { return *m_Object; }
		const T& operator*() const { return *m_Object; }

		// Comparison
		operator bool() { return m_Object != nullptr; }
		operator bool() const { return m_Object != nullptr; }

		bool operator==(const Ref<T>& other) const { return m_Object == other.m_Object; }
		bool operator!=(const Ref<T>& other) const { return !(*this == other); }

		// Create
		template<typename... Args>
		static Ref<T> Create(Args&&... args)
		{
			T* object = new T(std::forward<Args>(args)...);

			return Ref<T>(object);
		}

		// Cast to polymorphic type
		template<typename T2>
		Ref<T2> As() const
		{
			static_assert(std::is_base_of_v<T, T2> || std::is_base_of_v<T2, T>, "Class is not based on or used as base.");

			return Ref<T2>(*this);
		}

		uint32_t GetRefCount() const
		{
			if (m_Object)
				return m_Object->GetRefCount();
			else
				return 0;
		}

	private:
		// Increase ref count
		void IncRef() const
		{
			if (m_Object)
				m_Object->IncreaseRefCount();
		}

		// Decrease ref count
		void DecRef() const
		{
			if (m_Object)
			{
				m_Object->DecreaseRefCount();
			
				if (m_Object->GetRefCount() == 0)
				{
					delete m_Object;
					m_Object = nullptr;
				}
			}
		}

	private:
		mutable T* m_Object = nullptr;

		template<typename T2>
		friend class Ref;
	};
}
