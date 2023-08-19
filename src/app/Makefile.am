AM_CPPFLAGS = $(CXX17_FLAGS) -I$(srcdir)/../util -I$(srcdir)/../video
AM_CXXFLAGS = $(PICKY_CXXFLAGS)
BASE_LDADD = ../video/libvideo.a ../util/libutil.a \
	$(VPX_LIBS) $(SDL_LIBS) -lpthread


bin_PROGRAMS = udp_sender udp_receiver

udp_sender_SOURCES = udp_sender.cc \
	protocol.hh protocol.cc vp9_encoder.hh vp9_encoder.cc 
udp_sender_LDADD = $(BASE_LDADD)

udp_receiver_SOURCES = udp_receiver.cc \
	protocol.hh protocol.cc vp9_decoder.hh vp9_decoder.cc 
udp_receiver_LDADD = $(BASE_LDADD)
