#include "Support/EppoTest.h"

#include "Scripting/ScriptEngine.h"

using namespace Eppo;

// Pure (no .NET runtime) checks on the marshalling layout. ScriptFieldTypeSize is
// the native half of a contract shared with EppoScriptCore.ScriptGlue.FieldTypeSize;
// the widths must agree byte-for-byte or field/argument marshalling corrupts data.
TEST(ScriptMarshalling, ScriptFieldTypeSize_AllTypes_MatchManagedLayout)
{
    EXPECT_EQ(4u, ScriptFieldTypeSize(ScriptFieldType::Float));
    EXPECT_EQ(8u, ScriptFieldTypeSize(ScriptFieldType::Double));
    EXPECT_EQ(1u, ScriptFieldTypeSize(ScriptFieldType::Bool));
    EXPECT_EQ(2u, ScriptFieldTypeSize(ScriptFieldType::Char));
    EXPECT_EQ(2u, ScriptFieldTypeSize(ScriptFieldType::Int16));
    EXPECT_EQ(4u, ScriptFieldTypeSize(ScriptFieldType::Int32));
    EXPECT_EQ(8u, ScriptFieldTypeSize(ScriptFieldType::Int64));
    EXPECT_EQ(1u, ScriptFieldTypeSize(ScriptFieldType::Byte));
    EXPECT_EQ(2u, ScriptFieldTypeSize(ScriptFieldType::UInt16));
    EXPECT_EQ(4u, ScriptFieldTypeSize(ScriptFieldType::UInt32));
    EXPECT_EQ(8u, ScriptFieldTypeSize(ScriptFieldType::UInt64));
    EXPECT_EQ(8u, ScriptFieldTypeSize(ScriptFieldType::Vector2));
    EXPECT_EQ(12u, ScriptFieldTypeSize(ScriptFieldType::Vector3));
    EXPECT_EQ(16u, ScriptFieldTypeSize(ScriptFieldType::Vector4));
    EXPECT_EQ(8u, ScriptFieldTypeSize(ScriptFieldType::Entity));
    EXPECT_EQ(0u, ScriptFieldTypeSize(ScriptFieldType::None));
}

// Every field type must fit in the editor-time side-table buffer, and the
// typed Get/Set round-trip must be value-preserving.
TEST(ScriptMarshalling, ScriptFieldValue_TypedGetSet_PreservesValue)
{
    ScriptFieldValue value;
    value.Set(3.5f);
    EXPECT_NEAR(3.5f, value.Get<float>(), 1e-6f);

    value.Set<int64_t>(-1234567890123ll);
    EXPECT_TRUE(value.Get<int64_t>() == -1234567890123ll);
}
