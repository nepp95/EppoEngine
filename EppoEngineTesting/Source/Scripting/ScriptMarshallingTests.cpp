#include "Support/EppoTest.h"

#include "Scripting/ScriptEngine.h"

using namespace Eppo;

// Pure (no .NET runtime) checks on the marshalling layout. ScriptFieldTypeSize is
// the native half of a contract shared with EppoScriptCore.ScriptGlue.FieldTypeSize;
// the widths must agree byte-for-byte or field/argument marshalling corrupts data.
SUITE(ScriptMarshalling)
{
    TEST(ScriptFieldTypeSize_AllTypes_MatchManagedLayout)
    {
        CHECK_EQUAL(4u,  ScriptFieldTypeSize(ScriptFieldType::Float));
        CHECK_EQUAL(8u,  ScriptFieldTypeSize(ScriptFieldType::Double));
        CHECK_EQUAL(1u,  ScriptFieldTypeSize(ScriptFieldType::Bool));
        CHECK_EQUAL(2u,  ScriptFieldTypeSize(ScriptFieldType::Char));
        CHECK_EQUAL(2u,  ScriptFieldTypeSize(ScriptFieldType::Int16));
        CHECK_EQUAL(4u,  ScriptFieldTypeSize(ScriptFieldType::Int32));
        CHECK_EQUAL(8u,  ScriptFieldTypeSize(ScriptFieldType::Int64));
        CHECK_EQUAL(1u,  ScriptFieldTypeSize(ScriptFieldType::Byte));
        CHECK_EQUAL(2u,  ScriptFieldTypeSize(ScriptFieldType::UInt16));
        CHECK_EQUAL(4u,  ScriptFieldTypeSize(ScriptFieldType::UInt32));
        CHECK_EQUAL(8u,  ScriptFieldTypeSize(ScriptFieldType::UInt64));
        CHECK_EQUAL(8u,  ScriptFieldTypeSize(ScriptFieldType::Vector2));
        CHECK_EQUAL(12u, ScriptFieldTypeSize(ScriptFieldType::Vector3));
        CHECK_EQUAL(16u, ScriptFieldTypeSize(ScriptFieldType::Vector4));
        CHECK_EQUAL(8u,  ScriptFieldTypeSize(ScriptFieldType::Entity));
        CHECK_EQUAL(0u,  ScriptFieldTypeSize(ScriptFieldType::None));
    }

    // Every field type must fit in the editor-time side-table buffer, and the
    // typed Get/Set round-trip must be value-preserving.
    TEST(ScriptFieldValue_TypedGetSet_PreservesValue)
    {
        ScriptFieldValue value;
        value.Set(3.5f);
        CHECK_CLOSE(3.5f, value.Get<float>(), 1e-6f);

        value.Set<int64_t>(-1234567890123ll);
        CHECK(value.Get<int64_t>() == -1234567890123ll);
    }
}
