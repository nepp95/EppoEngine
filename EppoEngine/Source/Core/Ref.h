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
		// Default constructor
		Ref()
		{
			static_assert(std::is_base_of_v<RefCounter, T>, "Class is not based on RefCounter!");
		}

		// Destructor
		~Ref()
		{
			if (m_Object)
				DecRef();
		}
		
		// Construct from raw pointer
		Ref(T* object)
		{
			m_Object = object;
			IncRef();
		}

		// Copy constructor
		Ref(const Ref& other)
		{
			m_Object = other.m_Object;
			IncRef();
		}

		/*template<typename T2>
		Ref(const Ref<T2>& other)
		{
			if (m_Object)
				DecRef();

			m_Object = other.m_Object;
			IncRef();

			return *this;
		}*/

		// Copy assignment operator
		Ref& operator=(const Ref& other)
		{
			m_Object = other.m_Object;
			IncRef();
		}

		// Copy constructor child to base
		//template<typename T2>
		//Ref<T>& operator=(const Ref<T2>& other)
		//{
		//	if (m_Object)
		//		DecRef();

		//	m_Object = other.m_Object;
		//	IncRef();

		//	return *this;
		//}

		// Move constructor
		Ref(Ref&& other)
		{
			m_Object = other.m_Object;
			other.m_Object = nullptr;
		}

		// Move assignment operator
		Ref& operator=(Ref&& other)
		{
			m_Object = other.m_Object;
			other.m_Object = nullptr;

			return *this;
		}

		template<typename T2>
		Ref& operator=(Ref<T2>&& other)
		{
			static_assert(std::is_base_of_v<T, T2> || std::is_base_of_v<T2, T>, "Class is not based on or used as base.");

			m_Object = other.m_Object;
			other.m_Object = nullptr;

			return *this;
		}

		// Raw pointer
		const T* Raw() const
		{
			EPPO_ASSERT(m_Object);

			return m_Object;
		}

		// Pointer access
		T* operator->() const
		{
			EPPO_ASSERT(m_Object);

			return m_Object;
		}

		// Create
		template<typename... Args>
		static Ref<T> Create(Args&&... args)
		{
			T* object = new T(std::forward<Args>(args)...);

			return Ref<T>(object);
		}

		// Cast to polymorphic type
		template<typename T2>
		Ref<T2>& As()
		{
			static_assert(std::is_base_of_v<T, T2> || std::is_base_of_v<T2, T>, "Class is not based on or used as base.");
			EPPO_ASSERT(m_Object);

			T2* ptr = dynamic_cast<T2*>(m_Object);
			EPPO_ASSERT(ptr);
			
			return Ref<T2>(ptr);
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
			EPPO_ASSERT(m_Object);

			m_Object->IncreaseRefCount();
		}

		// Decrease ref count
		void DecRef() const
		{
			EPPO_ASSERT(m_Object);

			m_Object->DecreaseRefCount();

			if (m_Object->GetRefCount() == 0)
			{
				delete m_Object;
				m_Object = nullptr;
			}
		}

	private:
		mutable T* m_Object = nullptr;

		template<typename T2>
		friend class Ref;
	};
}
