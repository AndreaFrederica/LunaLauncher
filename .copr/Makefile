spec ?= packaging/fedora/lunalauncher.spec

.PHONY: srpm

srpm:
	git config submodule.libraries/quickjs-ng.url https://github.com/W-874/quickjs-ng.git
	git submodule update --init --recursive
	./packaging/fedora/make-srpm.sh "$(outdir)"
	find "$(outdir)/rpmbuild/SRPMS" -maxdepth 1 -name '*.src.rpm' -exec cp -a {} "$(outdir)/" \;
