#include "Test.h"

namespace Eppo
{
	// Dummy class
	class Base : public RefCounter
	{
	public:
		Base() = default;
		virtual ~Base() = default;

		uint32_t GetVar() const { return m_UselessVar; }

	private:
		uint32_t m_UselessVar = 27;
	};

	class Child : public Base
	{
	public:
		Child() = default;
		~Child() = default;

		uint32_t GetVar2() const { return m_UselessVar2; }

	private:
		uint32_t m_UselessVar2 = 72;
	};

	// Tests
	TEST(RefTest, Constructor)
	{
		Ref2<Base> ref = Ref2<Base>::Create();
		
		EXPECT_EQ(27, ref->GetVar());
		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());
	}

	TEST(RefTest, Destructor)
	{
		Ref2<Base> ref = Ref2<Base>::Create();

		EXPECT_EQ(27, ref->GetVar());
		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());

		{
			Ref2<Base> ref2 = ref;

			EXPECT_EQ(27, ref->GetVar());
			EXPECT_EQ(2, ref.GetRefCount());
			EXPECT_TRUE(ref.Raw());
			EXPECT_EQ(ref.Raw(), ref2.Raw());
		}

		EXPECT_EQ(27, ref->GetVar());
		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());
	}

	// TODO: What if ref count was higher?
	TEST(RefTest, CopyConstructor)
	{
		Ref2<Base> ref = Ref2<Base>::Create();

		EXPECT_EQ(27, ref->GetVar());
		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());

		{
			Ref2<Base> targetRef(ref);
			
			EXPECT_EQ(27, ref->GetVar());
			EXPECT_EQ(2, ref.GetRefCount());
			EXPECT_TRUE(ref.Raw());
			EXPECT_EQ(targetRef.Raw(), ref.Raw());
		}

		EXPECT_EQ(27, ref->GetVar());
		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());
	}

	// TODO: What if ref count was higher?
	TEST(RefTest, CopyAssignment)
	{
		Ref2<Base> ref = Ref2<Base>::Create();

		EXPECT_EQ(27, ref->GetVar());
		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());

		{
			Ref2<Base> targetRef = ref;

			EXPECT_EQ(27, ref->GetVar());
			EXPECT_EQ(2, ref.GetRefCount());
			EXPECT_TRUE(ref.Raw());
			EXPECT_EQ(targetRef.Raw(), ref.Raw());
		}

		EXPECT_EQ(27, ref->GetVar());
		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());
	}

	// TODO: What if ref count was higher?
	TEST(RefTest, MoveConstructor)
	{
		Ref2<Base> ref = Ref2<Base>::Create();

		EXPECT_EQ(27, ref->GetVar());
		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());

		Ref2<Base> targetRef(std::move(ref));

		EXPECT_EQ(27, targetRef->GetVar());
		EXPECT_FALSE(ref.Raw());
		EXPECT_EQ(1, targetRef.GetRefCount());
		EXPECT_TRUE(targetRef.Raw());
	}

	TEST(RefTest, MoveAssignment)
	{
		Ref2<Base> ref = Ref2<Base>::Create();

		EXPECT_EQ(27, ref->GetVar());
		EXPECT_EQ(1, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());

		Ref2<Base> targetRef = std::move(ref);

		EXPECT_EQ(27, targetRef->GetVar());
		EXPECT_FALSE(ref.Raw());
		EXPECT_EQ(1, targetRef.GetRefCount());
		EXPECT_TRUE(targetRef.Raw());
	}

	TEST(RefTest, CastToPolymorphicType)
	{
		Ref2<Base> ref;
	
		EXPECT_EQ(0, ref.GetRefCount());
		EXPECT_FALSE(ref.Raw());
	
		Ref2<Child> childRef = Ref2<Child>::Create();

		EXPECT_EQ(27, childRef->GetVar());
		EXPECT_EQ(72, childRef->GetVar2());
		EXPECT_EQ(1, childRef.GetRefCount());
		EXPECT_TRUE(childRef.Raw());

		ref = childRef;

		EXPECT_EQ(27, ref->GetVar());
		EXPECT_EQ(2, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());
		EXPECT_EQ(ref.Raw(), childRef.Raw());

		Ref2<Child> childRef2 = ref.As<Child>();

		EXPECT_EQ(27, childRef2->GetVar());
		EXPECT_EQ(72, childRef2->GetVar2());
		EXPECT_EQ(3, ref.GetRefCount());
		EXPECT_TRUE(ref.Raw());
		EXPECT_EQ(ref.Raw(), childRef.Raw());
		EXPECT_EQ(ref.Raw(), childRef2.Raw());
	}
}
