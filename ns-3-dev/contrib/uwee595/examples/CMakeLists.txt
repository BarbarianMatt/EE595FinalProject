build_lib_example(
  NAME link-performance
  SOURCE_FILES link-performance.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libmobility}
    ${libnetwork}
    ${libwifi}
    ${libsimplewireless}
)

build_lib_example(
  NAME single-bss-sld
  SOURCE_FILES single-bss-sld.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libmobility}
    ${libnetwork}
    ${libwifi}
    ${libsimplewireless}
)

build_lib_example(
  NAME single-bss-mld
  SOURCE_FILES single-bss-mld.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libmobility}
    ${libnetwork}
    ${libwifi}
    ${libsimplewireless}
)