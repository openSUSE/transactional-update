EXTRA_DIST = transactional-update-completion.bash

install-data-local:
	install -D -m 644 transactional-update-completion.bash $(DESTDIR)$(BASHCOMPLETIONDIR)/transactional-update
#	install -D -m 644 transactional-update-completion.zsh $(DESTDIR)$(datadir)/zsh/site-functions/_transactional-update
#	install -D -m 644 transactional-update-completion.fish $(DESTDIR)$(datadir)/fish/vendor_completions.d/transactional-update.fish

uninstall-local:
	rm -f $(DESTDIR)$(BASHCOMPLETIONDIR)/transactional-update
#	rm -f $(DESTDIR)$(datadir)/zsh/site-functions/_transactional-update
#	rm -f $(DESTDIR)$(datadir)/fish/vendor_completions.d/transactional-update.fish
