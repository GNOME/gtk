# NMake Makefile portion for generating Visual Studio
# projects and the other related items from a GIT checkout.
# Items in here should not need to be edited unless
# one is maintaining the NMake build files.

!ifndef PYTHON
PYTHON=python
!endif
!ifndef PERL
PERL=perl
!endif

NMAKE_ARGS = PYTHON=$(PYTHON) PERL=$(PERL)
!ifdef USE_EGL
NMAKE_ARGS = $(NMAKE_ARGS) USE_EGL=$(USE_EGL)
!endif
!ifdef FONT_FEATURES_DEMO
NMAKE_ARGS = $(NMAKE_ARGS) FONT_FEATURES_DEMO=$(FONT_FEATURES_DEMO)
!endif
!ifdef FONT_FEATURES_USE_PANGOFT2
NMAKE_ARGS = $(NMAKE_ARGS) FONT_FEATURES_USE_PANGOFT2=$(FONT_FEATURES_USE_PANGOFT2)
!endif

GENERATED_ITEMS =	\
	config-msvc.mak	\
	..\config.h.win32	\
	vs9\gtk3-version-paths.vsprops	\
	vs1x-props\gtk3-version-paths.props

all: bootstrap-msvc-projects

config-msvc.mak: config-msvc.mak.in ..\configure.ac gen-version-items.py
..\config.h.win32: ..\config.h.win32.in ..\configure.ac gen-version-items.py
vs9\gtk3-version-paths.vsprops: vs9\gtk3-version-paths.vsprops.in ..\configure.ac gen-version-items.py
vs1x-props\gtk3-version-paths.props: vs1x-props\gtk3-version-paths.props.in ..\configure.ac gen-version-items.py

config-msvc.mak	\
..\config.h.win32	\
vs9\gtk3-version-paths.vsprops	\
vs1x-props\gtk3-version-paths.props:
	@echo Generating $@...
	@$(PYTHON) .\gen-version-items.py --source=$@.in -o=$@

bootstrap-msvc-projects: $(GENERATED_ITEMS)
	$(MAKE) /f generate-msvc.mak $(NMAKE_ARGS) generate-broadway-items regenerate-all-msvc-projs

clean:
	@-for %%v in (11 12 14 15 16 17) do @for %%x in (sln vcxproj vcxproj.filters) do @del vs%%v\*.%%x
	@for %%x in (vcxproj vcxproj.filters) do @for %%f in (vs10\*.%%x) do @if exist %%fin del %%f
	@for %%x in (vcxproj vcxproj.filters) do @for %%f in (vs10\*.%%x) do @if exist %%fin del %%f
	@for %%x in (vcproj) do @for %%f in (vs9\*.%%x) do @if exist %%fin del %%f
	@-del ..\gdk\broadway\broadwayjs.h ..\gdk\broadway\clienthtml.h
	@-del ..\config.h.win32 config-msvc.mak
	@-del vs9\gtk3-version-paths.vsprops
	@-del vs1x-props\gtk3-version-paths.props
	@-for %%v in (9 10 11 12 14 15 16 17) do for %%d in (Debug Release Debug_Broadway Release_Broadway .vs) do rmdir /s/q vs%%v\%%d
	@-rmdir /s/q __pycache__
	@-del ..\gdk-*-build
	@-for %%v in (9 10 11 12 14 15 16 17) do for %%f in (vs%%v\*.user vs%%v\gtk+.vc.db vs%%v\gtk+.suo) do del /f %%f
