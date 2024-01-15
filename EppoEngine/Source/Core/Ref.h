#pragma once

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
		friend class Ref2;
		
	private:
		uint32_t m_RefCount = 0;
	};

	template<typename T>
	class Ref2
	{
	public:
		// Default constructor
		Ref2()
		{}

		// Destructor
		~Ref2()
		{
			if (m_Object)
				DecRef();
		}
		
		// Construct from raw pointer
		Ref2(T* object)
		{
			m_Object = object;
			IncRef();
		}

		// Copy constructor
		Ref2(const Ref2& other)
		{
			m_Object = other.m_Object;
			IncRef();
		}

		// Copy assignment operator
		Ref2& operator=(const Ref2& other)
		{
			m_Object = other.m_Object;
			IncRef();
		}

		// Copy constructor child to base
		template<typename T2>
		Ref2<T>& operator=(const Ref2<T2>& other)
		{
			if (m_Object)
				DecRef();

			m_Object = other.m_Object;
			IncRef();

			return *this;
		}

		// Move constructor
		Ref2(Ref2&& other) noexcept
		{
			m_Object = other.m_Object;
			other.m_Object = nullptr;
		}

		// Move assignment operator
		Ref2& operator=(Ref2&& other) noexcept
		{
			m_Object = other.m_Object;
			other.m_Object = nullptr;

			return *m_Object;
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
		static Ref2<T> Create(Args&&... args)
		{
			T* object = new T(std::forward<Args>(args)...);

			return Ref2<T>(object);
		}

		// Cast to polymorphic type
		template<typename T2>
		Ref2<T2>& As()
		{
			EPPO_ASSERT(m_Object);
			T2* ptr = dynamic_cast<T2*>(m_Object);
			EPPO_ASSERT(ptr);
			
			return Ref2<T2>(ptr);
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
		friend class Ref2;
	};
}
