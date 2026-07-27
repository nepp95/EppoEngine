#pragma once

namespace Eppo
{
    class Hash
    {
    public:
        static auto GenerateFnv(const std::string& contents) -> uint64_t;
    };
}