How to do a GTK release?
========================

## Before we begin

This document was last update for GTK 4.24. If that is too long ago,
it might be out of date. You may want to ask if anything changed
since then.

Our build uses Meson, and our release process relies on gitlab.gnome.org
and its CI. Make sure you have suitable versions of Meson and Ninja,
and an account on gitlab.gnome.org.

## Release check list

  0. Save all your work, then move to the branch from which you want
     to release. Go back to a pristine working directory. With Git,
     this works:

```sh
$ git clean -dfx
```

  1. Build using the common sequence:

```sh
$ meson setup _build
$ meson compile -C _build
```

  2. Update NEWS based on the content of git log; follow the format of prior
     entries. This includes finding noteworthy new features, collecting
     summaries for all the fixed bugs that are referenced and collecting all
     updated translations. The current best practice for this is to rely
     on gitlab-changelog.py to produce a rough draft, and then edit it.

```
$ gitlab_changelog.py gnome/gtk 4.23.2...
```

  3. Update the pot file and commit the changes:

```sh
$ ninja -C _build gtk40-pot
```

  4. If this is a major, stable, release, verify that the release notes
     in the API reference contain the relevant items.

  5. Verify that the version in `meson.build` is what you want it to be.
     The micro version should have been bumped after the lastrelease, so
     if the release you are making is a development snapshot or patch release,
     no further work is needed. If the release is a new stable release, change
     meson.build by bumping the minor version and resetting the micro version,
     i.e. go from 2.23.3 to 2.24.0. **Note**: this is critical, a slip-up here
     will cause the soname to change.

  6. If this is a devel release, make sure that the docs for new symbols are
     in good shape. Make sure that all new symbols have proper Since: tags
     and GDK_AVAILABLE_IN annotations.

  7. Commit the NEWS, meson.build and other changes on a branch and create
     a merge request for it. The established practice is to name the branch
     after the release: gtk-4-23-3, and use just the version number in the
     commit message: GTK 4.23.3. You can go ahead and add another commit
     on top that bumps the micro version number in meson.build, i.e. change
     from 4.23.3 to 4.23.4.

  8. Fix broken stuff found by 7), update the branch with fixes until the
     CI on the merge request is green.

  9. Merge the MR.

  10. Tag the commit with the version number in the name with a tag that
      also uses the version number, either using the git commandline, or
      using the gitlab web ui. *Note*: It is imperative that you tag
      the right commit - if you tag the one that bumps the version number,
      your release will be misnumbered.

```sh
$ git tag -m "GTK 4.23.3" 4.23.3 5336dcfe3cbc2e8
$ git push
```

  11. Thats it! The CI release service job gets triggered by the tag,
      produces the release tarball, and uploads it to the right location.
      If you don't trust it, check after a few minutes that it has appeared
      in https://download.gnome.org/sources/gtk/

  12. If you haven't done it as part of the release MR, bump the version number
      in `meson.build`, and commit that change as part of another MR.
