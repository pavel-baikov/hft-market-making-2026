file(GLOB CFG CONFIGURE_DEPENDS *.cfg *.cfg.in *.conf *.conf.in *.txt *.txt.in)

foreach(SRC ${CFG})
    get_filename_component(BASE ${SRC} NAME)
    string(REGEX REPLACE "\\.in$" "" TGT ${BASE})
    configure_file(${SRC} ${CMAKE_BINARY_DIR}/cfg/${TGT})
endforeach()
