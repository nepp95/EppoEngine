function conditional_project(name, condition)
    if condition then
        project (name)
    end
    return condition
end

-- https://github.com/premake/premake-core/issues/206
--
-- Usage example
--
-- if conditional_project("Conditional Project", os.target() == "windows") then
--     kind "SharedLib"
--     language "C++"
-- end