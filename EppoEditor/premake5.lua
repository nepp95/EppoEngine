
    filter {"system:windows", "configurations:Debug"}
        postbuildcommands {
            '{COPY} "%{DynamicLibrary.mono_debug}" "%{cfg.targetdir}"'
        }

    filter {"system:windows", "configurations:Release"}
        postbuildcommands {
            '{COPY} "%{DynamicLibrary.mono_release}" "%{cfg.targetdir}"'
        }

    filter {"system:windows", "configurations:Dist"}
        postbuildcommands {
            '{COPY} "%{DynamicLibrary.mono_release}" "%{cfg.targetdir}"'
        }
