add_rules("mode.debug", "mode.release")

target("ucp")
    set_kind("shared")
    add_files("src/**.c", "src/**.cpp")
    add_headerfiles("src/**.h", "src/**.hpp")

target("echo")
    set_kind("binary")
    add_files("examples/echo.cpp")
    add_deps("ucp")

target("udpserver")
    set_kind("binary")
    set_languages("cxx17")
    add_files("examples/udpserver.cpp")
    add_deps("ucp")

target("udpclient")
    set_kind("binary")
    set_languages("cxx17")
    add_files("examples/udpclient.cpp")
    add_deps("ucp")
    
    

