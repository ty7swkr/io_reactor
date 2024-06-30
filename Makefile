
TOPTARGETS := all clean
 
#SUBDIRS = reactor ssl_reactor tcp_reactor tcp_async_client https1_reactor http1_reactor websocket
SUBDIRS = websocket reactor tcp_reactor tcp_async_client ssl_reactor http1_reactor https1_reactor
 
$(TOPTARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)
 
.PHONY: $(TOPTARGETS) $(SUBDIRS)

