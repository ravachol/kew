SBLG = sblg
LOWDOWN = lowdown
PREFIX = ../pages
PAGES_SRC_DIR = site_src
PAGES_SRC = $(wildcard $(PAGES_SRC_DIR)/*.md)
PAGES = $(subst $(PAGES_SRC_DIR)/,,$(PAGES_SRC:.md=.html))

all: $(PAGES)

$(PAGES): template.xml $(PAGES_SRC)
	cp $(addprefix $(PAGES_SRC_DIR)/,$(@:.html=.md)) .
	echo "<article data-sblg-article=\"1\" data-sblg-set-title=\"kew &#40;&#47;kjuː&#47;, a variant of cue&#41; is a terminal music player.\" data-sblg-set-subtitle=\"kew &#40;&#47;kjuː&#47;&#41; is a variant of cue\" data-sblg-set-tagline=\"Music for the Shell\">"  >$(@:.html=.xml)
	$(LOWDOWN) --html-no-head-ids  --html-no-skiphtml --html-no-escapehtml $(@:.html=.md) >>$(@:.html=.xml)
	echo "</article>" >>$(@:.html=.xml)
	$(SBLG) -o $@ -t template.xml -c $(@:.html=.xml)
	rm -f $(@:.html=.md) $(@:.html=.xml)

install:  $(PAGES) images css
	mkdir -p $(PREFIX)
	install -m0444 *.html $(PREFIX)
	cp -p -r css $(PREFIX)
	cp -p -r images $(PREFIX)
	rm *.html

clean:
	rm *.html
