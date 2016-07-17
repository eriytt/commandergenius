#INCLUDES =                                      \
        @KDRIVE_PURE_INCS@                              \
        @KDRIVE_CFLAGS@ \
        @VRX_INCS@

#bin_PROGRAMS = vrx

# vrx_SOURCES = android_main.c vrx.c vrxinit.c kbd.c mouse.c os.c fake.h

# vrx_LDADD = \
#         @KDRIVE_LIBS@\
#         @VRX_LIBS@

# vrx_DEPENDENCIES =  @KDRIVE_LOCAL_LIBS@

# vrx_LDFLAGS = $(LD_EXPORT_SYMBOLS_FLAG)

lib_LTLIBRARIES = libvrxwm.la

libvrxwm_la_SOURCES = vrx_jni.cc  vrx_renderer.cc vrx_renderer.h  world_layout_data.h

relink:
	rm -f $(noinst_LTLIBRARIES) && make $(noinst_LTLIBRARIES)





