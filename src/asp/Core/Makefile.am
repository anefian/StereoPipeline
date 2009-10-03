#########################################################################
# sources
#########################################################################

if MAKE_MODULE_CORE

include_HEADERS = BlobIndexThreaded.h StereoSettings.h SparseView.h      \
		  ControlNetworkLoader.h DiskImageResourceDDD.h          \
		  InpaintView.h MedianFilter.h SparseView.h              \
		  OrthoRasterizer.h SoftwareRenderer.h 

libaspCore_la_SOURCES = BlobIndexThreaded.cc BundleAdjustUtils.cc        \
		  ControlNetworkLoader.cc DiskImageResourceDDD.cc        \
		  SoftwareRenderer.cc StereoSettings.cc

libaspCore_la_LIBADD = @MODULE_CORE_LIBS@

lib_LTLIBRARIES = libaspCore.la

endif

#########################################################################
# general
#########################################################################

AM_CPPFLAGS = @ASP_CPPFLAGS@
AM_LDFLAGS = @ASP_LDFLAGS@ -version-info @LIBTOOL_VERSION@

SUBDIRS = .

includedir = $(prefix)/include/asp/Core

include $(top_srcdir)/config/rules.mak