#!/usr/bin/env python
import os
import sys

libname = "russian_roguelite"
projectdir = "project"

local_env = Environment(tools=["default"], PLATFORM="")
env = local_env.Clone()

if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print("godot-cpp is missing.")
    print("Run: git clone https://github.com/godotengine/godot-cpp.git")
    sys.exit(1)

env = SConscript("godot-cpp/SConstruct", {"api_version": 4.7, "env": env})
env.Append(CPPPATH=["src/"])
sources = (
    Glob("src/*.cpp") +
    Glob("src/player/*.cpp") +
    Glob("src/enemies/*.cpp") +
    Glob("src/skills/*.cpp")
)

suffix = env["suffix"].replace(".dev", "").replace(".universal", "")
lib_filename = "{}{}{}{}".format(
    env.subst("$SHLIBPREFIX"),
    libname,
    suffix,
    env.subst("$SHLIBSUFFIX"),
)

library = env.SharedLibrary(
    "bin/{}/{}".format(env["platform"], lib_filename),
    source=sources,
)
copy = env.Install("{}/bin/{}/".format(projectdir, env["platform"]), library)
Default([library, copy])
