AM_CPPFLAGS = $(CXX17_FLAGS) -I$(srcdir)/../util
AM_CXXFLAGS = $(PICKY_CXXFLAGS)
BASE_LDADD = ../util/libutil.a \
	$(OPUS_LIBS) -lpthread

bin_PROGRAMS = tester

tester_SOURCES = tester.cc \
	opus_wrapper.hh opus_wrapper.cc
tester_LDADD = $(BASE_LDADD)


# noinst_LIBRARIES = libaudio.a

# libaudio_a_SOURCES = \
# 	opus_wrapper.hh opus_wrapper.cc

# bin_PROGRAMS = audio_encode

# audio_encode_SOURCES = audio_encode.cc
# audio_encode_LDADD = libaudio.a ../util/libutil.a $(OPUS_LIBS) -lpthread

