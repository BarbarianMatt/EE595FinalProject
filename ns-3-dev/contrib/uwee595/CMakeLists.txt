set(source_files
    model/two-state-propagation-loss-model.cc
    model/snr-per-error-model.cc
    model/simple-wireless-net-device.cc
    model/simple-wireless-channel.cc
    model/bernoulli_packet_socket_client.cc
    )

set(header_files
    model/two-state-propagation-loss-model.h
    model/snr-per-error-model.h
    model/simple-wireless-channel.h
    model/simple-wireless-net-device.h
    model/bernoulli_packet_socket_client.h
    )


build_lib(
    LIBNAME simplewireless
    SOURCE_FILES ${source_files}
    HEADER_FILES ${header_files}
    LIBRARIES_TO_LINK
    ${libapplications}
    ${libcore}
    ${libcsma}
    ${libenergy}
    ${libmobility}
    ${libnetwork}
    ${libpoint-to-point}
    ${libsip}
    ${libwifi}
)