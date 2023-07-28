AM_CPPFLAGS = $(CXX17_FLAGS) -I$(srcdir)/../util -I$(srcdir)/../video
AM_CXXFLAGS = $(PICKY_CXXFLAGS)
BASE_LDADD = ../video/libvideo.a ../util/libutil.a \
	$(VPX_LIBS) $(SDL_LIBS) -lpthread


bin_PROGRAMS = sender receiver

sender_SOURCES = sender.cc \
	protocol.hh protocol.cc video_encoder.hh video_encoder.cc 
sender_LDADD = $(BASE_LDADD)

receiver_SOURCES = receiver.cc \
	protocol.hh protocol.cc video_decoder.hh video_decoder.cc 
receiver_LDADD = $(BASE_LDADD)
