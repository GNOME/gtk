## GTK CI infrastructure

GTK uses different CI images depending on platform and jobs.

The CI images are Docker containers, generated either using `docker` or
`podman`, and pushed to the GitLab [container registry][registry].

Each Docker image has a tag composed of two parts:

 - `${image}`: the base image for a given platform, like "fedora" or
   "debian-stable"
 - `${number}`: an incremental version number, or `latest`

See the [container registry][registry] for the available images for each
branch, as well as their available versions.

Note that using `latest` as version number will overwrite the most
recently uploaded image in the registry.

### Checklist for Updating a CI image

 - [ ] Update FDO_DISTRIBUTION_VERSION if needed
 - [ ] Update FDO_DISTRIBUTION_PACKAGES with the dependencies
 - [ ] Update FDO_DISTRIBUTION_EXEC with new image setup commands
 - [ ] Bump BASE_TAG
 - [ ] Commit
 - [ ] Open a merge request with your changes and let it run

### Checklist for Adding a new CI image

 - [ ] Add a new `container:{platform}` job in the `prepare` stage, see
   others for reference. This job must extend
   `.fdo.container-build@{distro}@{arch}` directly or indirectly
 - [ ] Add a `.distribution.{platform}` job extending
   `.fdo.distribution-image@{distro}`
 - [ ] Make every job using this image extend `.distribution.{platform}`
   and need `container:{platform}`
 - [ ] Commit
 - [ ] Open a merge request with your changes and let it run

### Checklist for Adding a new dependency to a CI image

Our images are layered, and the base (called fedora-base) contains
all the rpm payload. Therefore, adding a new dependency is a 2-step
process:

 1. [ ] Build and upload fedora-base:$version+1
 1. [ ] Build and upload fedora:$version+1 based on fedora-base:version+1

[registry]: https://gitlab.gnome.org/GNOME/gtk/container_registry
