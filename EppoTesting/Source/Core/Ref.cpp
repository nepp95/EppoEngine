#include "Test.h"

namespace Eppo
{
	// Dummy class
	class Reffie : public RefCounter
	{
	public:
		Reffie() = default;
		~Reffie() = default;

		uint32_t GetVar() const { return m_UselessVar; }

	private:
		uint32_t m_UselessVar = 27;
	};

	// Tests
	TEST(RefTest, Constructor)
	{
		Ref2<Reffie> ref;
		
		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());
	}

	// TODO: What if ref count was higher?
	TEST(RefTest, Destructor)
	{
		{
			Ref2<Reffie> ref;

			EXPECT_EQ(1, ref.GetRefCount());
			EXPECT_TRUE(ref.Raw());
		}
	}

	// TODO: What if ref count was higher?
	TEST(RefTest, CopyConstructor)
	{
		Ref2<Reffie> ref;

		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());

		{
			Ref2<Reffie> targetRef(ref);
			
			EXPECT_EQ(2, ref.GetRefCount());
			EXPECT_TRUE(ref.Raw());
			EXPECT_EQ(targetRef.Raw(), ref.Raw());
		}

		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());
	}

	// TODO: What if ref count was higher?
	TEST(RefTest, CopyAssignment)
	{
		Ref2<Reffie> ref;

		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());

		{
			Ref2<Reffie> targetRef = ref;

			EXPECT_EQ(2, ref.GetRefCount());
			EXPECT_TRUE(ref.Raw());
			EXPECT_EQ(targetRef.Raw(), ref.Raw());
		}

		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());
	}

	// TODO: What if ref count was higher?
	TEST(RefTest, MoveConstructor)
	{
		Ref2<Reffie> ref;

		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());

		Ref2<Reffie> targetRef(std::move(ref));

		EXPECT_FALSE(ref.Raw());

		EXPECT_EQ(1, targetRef.GetRefCount());
		EXPECT_TRUE(targetRef.Raw());
	}

	TEST(RefTest, MoveAssignment)
	{
		Ref2<Reffie> ref;

		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());

		Ref2<Reffie> targetRef = std::move(ref);

		EXPECT_FALSE(ref.Raw());

		EXPECT_EQ(1, targetRef.GetRefCount());
		EXPECT_TRUE(targetRef.Raw());
	}
}
