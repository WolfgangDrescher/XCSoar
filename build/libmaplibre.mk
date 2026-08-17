# Support for the MapLibre Native basemap renderer (experimental).
#
# Enable with "make MAPLIBRE=y"; requires an OpenGL target.  Without
# an externally given MAPLIBRE_PREFIX, MapLibre Native is built
# automatically by build/maplibre.sh into the target output directory
# (same pattern as ANGLE, see build/angle.mk; a tarball-based
# build/python/build/libs.py project is not possible because upstream
# publishes no source tarball containing its git submodules).

MAPLIBRE ?= n

ifeq ($(MAPLIBRE),y)

ifneq ($(OPENGL),y)
$(error MAPLIBRE=y requires an OpenGL target)
endif

ifeq ($(MAPLIBRE_PREFIX),)
# no prefix given: build MapLibre Native ourselves
MAPLIBRE_PREFIX = $(TARGET_OUTPUT_DIR)/maplibre
MAPLIBRE_BUILD_STAMP = $(MAPLIBRE_PREFIX)/.stamp

$(MAPLIBRE_BUILD_STAMP): $(topdir)/build/maplibre.sh
	@$(NQ)echo "  BUILD   MapLibre Native"
	$(Q)$(topdir)/build/maplibre.sh $(MAPLIBRE_PREFIX) $(TARGET)
	$(Q)touch $@

compile-depends += $(MAPLIBRE_BUILD_STAMP)
endif

MAPLIBRE_CPPFLAGS = -isystem $(MAPLIBRE_PREFIX)/include

# link mbgl-core and all its vendored static libraries; ld64 resolves
# them in any order, GNU ld needs a group.  Deliberately recursively
# expanded ("=") so the wildcard runs at link time, after the
# automatic MapLibre build has produced the libraries.
MAPLIBRE_STATIC_LIBS = $(wildcard $(MAPLIBRE_PREFIX)/lib/*.a)
ifeq ($(TARGET_IS_DARWIN),y)
MAPLIBRE_LDLIBS = $(MAPLIBRE_STATIC_LIBS)

# frameworks and system libraries used by MapLibre's darwin platform
# code (see platform/darwin/darwin.cmake)
MAPLIBRE_LDLIBS += -framework Foundation -framework CoreFoundation \
	-framework CoreGraphics -framework CoreLocation \
	-framework CoreImage -framework SystemConfiguration \
	-lsqlite3 -lz
else
MAPLIBRE_LDLIBS = -Wl,--start-group $(MAPLIBRE_STATIC_LIBS) -Wl,--end-group

# system libraries used by MapLibre's Linux platform code
MAPLIBRE_LDLIBS += -lwebp -luv -licuuc -licui18n -licudata
endif

# The define is global (TARGET_CPPFLAGS) because it affects the
# layout of MapSettings, which is included nearly everywhere.
TARGET_CPPFLAGS += -DENABLE_MAPLIBRE

endif
