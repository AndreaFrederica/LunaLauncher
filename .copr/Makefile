spec ?= packaging/fedora/lunalauncher.spec
outdir ?= $(CURDIR)/dist/copr
srpm_tools ?= git rpmbuild tar gzip python3

.PHONY: srpm deps

# Copr runs this target as root inside a make-srpm chroot that only adds
# `make` and `dnf` on top of Fedora's @buildsys-build group. The SRPM helper
# needs git, rpmbuild, tar, gzip and python3, which that group does not
# guarantee. Install them only when absent, so local runs stay unprivileged.
deps:
	@missing="$$(for tool in $(srpm_tools); do command -v "$$tool" >/dev/null 2>&1 || printf '%s ' "$$tool"; done)"; \
	if [ -n "$$missing" ]; then \
	    echo "Installing missing SRPM tools: $$missing"; \
	    dnf -y install git rpm-build tar gzip python3; \
	fi

srpm: deps
	git submodule sync --recursive
	git submodule update --init --recursive
	./packaging/fedora/make-srpm.sh "$(outdir)"
	find "$(outdir)/rpmbuild/SRPMS" -maxdepth 1 -name '*.src.rpm' -exec cp -a {} "$(outdir)/" \;
