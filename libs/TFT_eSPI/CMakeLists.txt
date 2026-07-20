
idf_component_register(SRCS "TFT_eSPI.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES arduino)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-misleading-indentation)