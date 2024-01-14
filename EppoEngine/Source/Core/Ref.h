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
		{
			m_Object = new T();
			IncRef();
		}

		// Destructor
		~Ref2()
		{
			DecRef();
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
			return m_Object;
		}

		uint32_t GetRefCount() const
		{
			return m_Object->GetRefCount();
		}

	private:
		// Increase ref count
		void IncRef() const
		{
			m_Object->IncreaseRefCount();
		}

		// Decrease ref count
		void DecRef() const
		{
			m_Object->DecreaseRefCount();

			if (m_Object->GetRefCount() == 0)
			{
				delete m_Object;
				m_Object = nullptr;
			}
		}

	private:
		mutable T* m_Object;
	};
}
